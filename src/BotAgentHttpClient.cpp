#include "BotAgentHttpClient.h"
#include "BotAgentConfig.h"
#include "Log.h"

#include "httplib.h"

#include <string>

namespace
{
    // Splits "http://host:port/path/here" into ("http://host:port", "/path/here").
    // Falls back to path "/" when none is present.
    bool SplitUrl(std::string const& url, std::string& base, std::string& path)
    {
        auto schemeEnd = url.find("://");
        if (schemeEnd == std::string::npos)
            return false;

        auto pathStart = url.find('/', schemeEnd + 3);
        if (pathStart == std::string::npos)
        {
            base = url;
            path = "/";
        }
        else
        {
            base = url.substr(0, pathStart);
            path = url.substr(pathStart);
        }
        return true;
    }
}

BotAgentHttpClient& BotAgentHttpClient::Instance()
{
    static BotAgentHttpClient instance;
    return instance;
}

void BotAgentHttpClient::Start()
{
    if (_running.exchange(true))
        return;

    _worker = std::thread([this]() { WorkerLoop(); });
}

void BotAgentHttpClient::Stop()
{
    if (!_running.exchange(false))
        return;

    _cv.notify_all();
    if (_worker.joinable())
        _worker.join();

    // Drop any jobs that were still queued.
    std::lock_guard<std::mutex> lock(_mutex);
    std::queue<std::pair<std::string, std::string>> empty;
    std::swap(_jobs, empty);
}

void BotAgentHttpClient::PostJson(std::string url, std::string jsonBody)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _jobs.emplace(std::move(url), std::move(jsonBody));
    }
    _cv.notify_one();
}

void BotAgentHttpClient::WorkerLoop()
{
    while (_running.load())
    {
        std::pair<std::string, std::string> job;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cv.wait(lock, [this]() { return !_running.load() || !_jobs.empty(); });
            if (!_running.load() && _jobs.empty())
                return;
            if (_jobs.empty())
                continue;
            job = std::move(_jobs.front());
            _jobs.pop();
        }

        Send(job.first, job.second);
    }
}

void BotAgentHttpClient::Send(std::string const& url, std::string const& body)
{
    std::string base, path;
    if (!SplitUrl(url, base, path))
    {
        LOG_ERROR("module.bot_agent", "[bot-agent] malformed outbound URL: {}", url);
        return;
    }

    httplib::Client cli(base.c_str());
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(5, 0);
    cli.set_write_timeout(5, 0);

    httplib::Headers headers;
    std::string const& token = BotAgentConfig::Instance().Token;
    if (!token.empty())
        headers.emplace("X-Agent-Token", token);

    auto res = cli.Post(path.c_str(), headers, body, "application/json");
    if (!res)
    {
        LOG_WARN("module.bot_agent",
            "[bot-agent] outbound POST to {} failed: {}", url, httplib::to_string(res.error()));
    }
    else if (res->status >= 400)
    {
        LOG_WARN("module.bot_agent",
            "[bot-agent] outbound POST to {} returned HTTP {}", url, res->status);
    }
}
