#include "BotAgentHttpServer.h"
#include "BotAgentBuyAction.h"
#include "BotAgentChat.h"
#include "BotAgentConfig.h"
#include "BotAgentJson.h"
#include "BotAgentReads.h"
#include "BotAgentSellAction.h"
#include "BotAgentTaskQueue.h"
#include "BotAgentTradeAction.h"
#include "Log.h"

// Single-header HTTP library (vendored under module/deps).
#include "httplib.h"

#include <cstdint>
#include <string>

namespace
{
    // Reads marshal onto the world thread; they finish in well under this.
    constexpr uint32 READ_TIMEOUT_MS = 3000;
    // Starting a buy (validate item + nearest vendor + suspend AI) is quick too;
    // the long travel happens asynchronously after the call returns.
    constexpr uint32 BUY_START_TIMEOUT_MS = 5000;

    // Parses a low-GUID captured from the route regex; 0 on failure.
    uint32 ParseGuid(std::string const& s)
    {
        try { return static_cast<uint32>(std::stoul(s)); }
        catch (...) { return 0; }
    }
}

namespace
{
    // Returns true if the request is authorised. When a token is configured,
    // the request must carry it in the X-Agent-Token header.
    bool RequireToken(httplib::Request const& req, httplib::Response& res)
    {
        std::string const& token = BotAgentConfig::Instance().Token;
        if (token.empty())
            return true; // unauthenticated mode (a startup warning was logged)

        if (req.get_header_value("X-Agent-Token") == token)
            return true;

        res.status = 401;
        res.set_content(R"({"error":"unauthorized"})", "application/json");
        return false;
    }
}

BotAgentHttpServer& BotAgentHttpServer::Instance()
{
    static BotAgentHttpServer instance;
    return instance;
}

BotAgentHttpServer::~BotAgentHttpServer()
{
    Stop();
}

void BotAgentHttpServer::Start()
{
    if (_running.exchange(true))
        return; // already started

    _server = std::make_unique<httplib::Server>();
    RegisterRoutes();

    BotAgentConfig const& cfg = BotAgentConfig::Instance();
    std::string host = cfg.BindAddress;
    uint16 port = cfg.Port;

    // Bind synchronously so we can report a bind failure immediately.
    if (!_server->bind_to_port(host.c_str(), port))
    {
        LOG_ERROR("module.bot_agent",
            "[bot-agent] failed to bind HTTP server to {}:{}", host, port);
        _running = false;
        _server.reset();
        return;
    }

    LOG_INFO("module.bot_agent", "[bot-agent] HTTP server listening on {}:{}", host, port);

    _thread = std::thread([this]()
    {
        // Blocks until stop() is called from another thread.
        _server->listen_after_bind();
    });
}

void BotAgentHttpServer::Stop()
{
    if (!_running.exchange(false))
        return;

    if (_server)
        _server->stop();

    if (_thread.joinable())
        _thread.join();

    _server.reset();
    LOG_INFO("module.bot_agent", "[bot-agent] HTTP server stopped");
}

void BotAgentHttpServer::RegisterRoutes()
{
    // Liveness probe — intentionally unauthenticated.
    _server->Get("/health", [](httplib::Request const&, httplib::Response& res)
    {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // --- live reads -------------------------------------------------------
    // GET /bot/<guid>/gold | /level | /inventory
    auto readRoute = [](std::string (*fn)(uint32))
    {
        return [fn](httplib::Request const& req, httplib::Response& res)
        {
            if (!RequireToken(req, res))
                return;

            uint32 guid = ParseGuid(req.matches[1].str());
            if (!guid)
            {
                res.status = 400;
                res.set_content(BotAgentJson::Error("invalid_guid"), "application/json");
                return;
            }

            std::string body = BotAgentTaskQueue::RunSync(
                [guid, fn]() { return fn(guid); }, READ_TIMEOUT_MS);
            res.set_content(body, "application/json");
        };
    };

    _server->Get(R"(/bot/(\d+)/gold)",      readRoute(&BotAgentReads::Gold));
    _server->Get(R"(/bot/(\d+)/level)",     readRoute(&BotAgentReads::Level));
    _server->Get(R"(/bot/(\d+)/inventory)", readRoute(&BotAgentReads::Inventory));

    // --- actions ----------------------------------------------------------
    // POST /bot/<guid>/buy  { "item_name": "...", "radius": <optional int> }
    _server->Post(R"(/bot/(\d+)/buy)", [](httplib::Request const& req, httplib::Response& res)
    {
        if (!RequireToken(req, res))
            return;

        uint32 guid = ParseGuid(req.matches[1].str());
        if (!guid)
        {
            res.status = 400;
            res.set_content(BotAgentJson::Error("invalid_guid"), "application/json");
            return;
        }

        std::string itemName;
        if (!BotAgentJson::GetString(req.body, "item_name", itemName) || itemName.empty())
        {
            res.status = 400;
            res.set_content(BotAgentJson::Error("missing_item_name"), "application/json");
            return;
        }
        uint32 radius = BotAgentJson::GetUInt(req.body, "radius", 0);

        std::string body = BotAgentTaskQueue::RunSync(
            [guid, itemName, radius]() { return BotAgentBuyAction::Start(guid, itemName, radius); },
            BUY_START_TIMEOUT_MS);
        res.set_content(body, "application/json");
    });

    // POST /bot/<guid>/sell  { "items": "Name1|Name2", "radius": <optional int> }
    _server->Post(R"(/bot/(\d+)/sell)", [](httplib::Request const& req, httplib::Response& res)
    {
        if (!RequireToken(req, res))
            return;

        uint32 guid = ParseGuid(req.matches[1].str());
        if (!guid)
        {
            res.status = 400;
            res.set_content(BotAgentJson::Error("invalid_guid"), "application/json");
            return;
        }

        std::string items;
        if (!BotAgentJson::GetString(req.body, "items", items) || items.empty())
        {
            res.status = 400;
            res.set_content(BotAgentJson::Error("missing_items"), "application/json");
            return;
        }
        uint32 radius = BotAgentJson::GetUInt(req.body, "radius", 0);

        std::string body = BotAgentTaskQueue::RunSync(
            [guid, items, radius]() { return BotAgentSellAction::Start(guid, items, radius); },
            BUY_START_TIMEOUT_MS);
        res.set_content(body, "application/json");
    });

    // POST /bot/<guid>/trade  { "player_guid": <int>, "items": "Name1|Name2" }
    _server->Post(R"(/bot/(\d+)/trade)", [](httplib::Request const& req, httplib::Response& res)
    {
        if (!RequireToken(req, res))
            return;

        uint32 guid = ParseGuid(req.matches[1].str());
        if (!guid)
        {
            res.status = 400;
            res.set_content(BotAgentJson::Error("invalid_guid"), "application/json");
            return;
        }

        uint32 playerGuid = BotAgentJson::GetUInt(req.body, "player_guid", 0);
        std::string items;
        BotAgentJson::GetString(req.body, "items", items);
        if (!playerGuid || items.empty())
        {
            res.status = 400;
            res.set_content(BotAgentJson::Error("missing_player_or_items"), "application/json");
            return;
        }

        std::string body = BotAgentTaskQueue::RunSync(
            [guid, playerGuid, items]() { return BotAgentTradeAction::Start(guid, playerGuid, items); },
            BUY_START_TIMEOUT_MS);
        res.set_content(body, "application/json");
    });

    // POST /chat  { "group_guid": "<group low id>", "text": "..." }
    _server->Post("/chat", [](httplib::Request const& req, httplib::Response& res)
    {
        if (!RequireToken(req, res))
            return;

        std::string groupStr, text;
        BotAgentJson::GetString(req.body, "group_guid", groupStr);
        BotAgentJson::GetString(req.body, "text", text);

        uint32 groupId = ParseGuid(groupStr);
        if (!groupId || text.empty())
        {
            res.status = 400;
            res.set_content(BotAgentJson::Error("invalid_group_or_text"), "application/json");
            return;
        }

        std::string body = BotAgentTaskQueue::RunSync(
            [groupId, text]() { return BotAgentChat::SendPartyMessage(groupId, text); },
            READ_TIMEOUT_MS);
        res.set_content(body, "application/json");
    });
}
