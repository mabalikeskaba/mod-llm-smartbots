#include "BotAgentBuyAction.h"
#include "BotAgentBots.h"
#include "BotAgentConfig.h"
#include "BotAgentHttpClient.h"
#include "BotAgentJson.h"
#include "BotAgentVendorIndex.h"

#include "Creature.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "QueryResult.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "MotionMaster.h"
#include "Player.h"
#include "SharedDefines.h"

// mod-playerbots — bot AI control (strategy suspend/restore).
#include "Playerbots.h"
#include "PlayerbotAI.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace
{
    constexpr uint32 MOVE_POINT_ID      = 0xB07;
    constexpr float  ARRIVE_DISTANCE    = 5.0f;   // interaction range (yards)
    constexpr float  VENDOR_FIND_RANGE  = 10.0f;  // search radius at the destination

    enum class BuyState { Travel, Buy };

    struct ActiveBuy
    {
        std::string              requestId;
        uint32                   botGuid    = 0;
        uint32                   itemEntry  = 0;
        std::string              itemName;
        uint32                   vendorEntry = 0;
        uint16                   vendorMap   = 0;
        float                    vx = 0, vy = 0, vz = 0;
        std::vector<std::string> savedStrategies;
        BuyState                 state       = BuyState::Travel;
        uint32                   elapsedMs   = 0;
        uint32                   timeoutMs   = 120000;
    };

    // World-thread-only state — no locking required.
    std::vector<ActiveBuy>      g_active;
    std::unordered_set<uint32>  g_busyBots;
    uint64                      g_counter = 0;

    std::string NextRequestId()
    {
        return "buy-" + std::to_string(++g_counter);
    }

    std::string SqlEscape(std::string const& in)
    {
        std::string out;
        out.reserve(in.size());
        for (char c : in)
        {
            if (c == '\'')      out += "''";
            else if (c == '\\') {} // drop backslashes
            else                out += c;
        }
        return out;
    }

    struct ResolvedItem { uint32 entry = 0; std::string name; };

    // Resolve an item by name (exact, then substring) via the world DB.
    ResolvedItem ResolveItemByName(std::string const& name)
    {
        std::string esc = SqlEscape(name);

        QueryResult r = WorldDatabase.Query(
            "SELECT entry, name FROM item_template WHERE name = '{}' LIMIT 1", esc);
        if (!r)
            r = WorldDatabase.Query(
                "SELECT entry, name FROM item_template WHERE name LIKE '%{}%' LIMIT 1", esc);

        ResolvedItem out;
        if (r)
        {
            Field* f = r->Fetch();
            out.entry = f[0].Get<uint32>();
            out.name  = f[1].Get<std::string>();
        }
        return out;
    }

    std::vector<std::string> SuspendAI(PlayerbotAI* botAI)
    {
        std::vector<std::string> saved = botAI->GetStrategies(BOT_STATE_NON_COMBAT);
        botAI->ChangeStrategy("-follow,-grind,+passive", BOT_STATE_NON_COMBAT);
        return saved;
    }

    void RestoreAI(PlayerbotAI* botAI, std::vector<std::string> const& saved)
    {
        botAI->ClearStrategies(BOT_STATE_NON_COMBAT);
        for (std::string const& s : saved)
            botAI->ChangeStrategy("+" + s, BOT_STATE_NON_COMBAT);
    }

    // Sends the result to the C# service and clears the action's bookkeeping.
    // `bot`/`botAI` may be null if the bot vanished (then AI is not restored).
    void Finalize(ActiveBuy const& a, Player* bot, PlayerbotAI* botAI,
                  bool success, std::string const& item, uint32 priceCopper,
                  std::string const& vendor, std::string const& message)
    {
        if (bot && botAI)
            RestoreAI(botAI, a.savedStrategies);

        std::string body = "{";
        body += "\"request_id\":\"" + BotAgentJson::Escape(a.requestId) + "\",";
        body += "\"success\":" + std::string(success ? "true" : "false") + ",";
        body += "\"item\":\"" + BotAgentJson::Escape(item) + "\",";
        body += "\"price\":" + std::to_string(priceCopper) + ",";
        body += "\"vendor\":\"" + BotAgentJson::Escape(vendor) + "\",";
        body += "\"message\":\"" + BotAgentJson::Escape(message) + "\"";
        body += "}";

        std::string url = BotAgentConfig::Instance().CallbackBaseUrl + "/action_result";
        BotAgentHttpClient::Instance().PostJson(url, std::move(body));

        g_busyBots.erase(a.botGuid);
    }
}

std::string BotAgentBuyAction::Start(uint32 botGuidLow, std::string const& itemName, uint32 radiusYards)
{
    if (g_busyBots.count(botGuidLow))
        return R"({"accepted":false,"reason":"busy"})";

    Player* bot = BotAgent::FindBotPlayer(botGuidLow);
    if (!bot)
        return R"({"accepted":false,"reason":"bot_not_in_world"})";

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return R"({"accepted":false,"reason":"not_a_bot"})";

    ResolvedItem item = ResolveItemByName(itemName);
    if (!item.entry)
        return R"({"accepted":false,"reason":"unknown_item"})";

    std::vector<BotAgentVendorIndex::VendorCandidate> vendors =
        BotAgentVendorIndex::FindNearest(
            item.entry, static_cast<uint16>(bot->GetMapId()),
            bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
            radiusYards, 25);

    if (vendors.empty())
        return R"({"accepted":false,"reason":"no_vendor_on_map"})";

    BotAgentVendorIndex::VendorCandidate const& v = vendors.front();

    ActiveBuy a;
    a.requestId   = NextRequestId();
    a.botGuid     = botGuidLow;
    a.itemEntry   = item.entry;
    a.itemName    = item.name;
    a.vendorEntry = v.vendorEntry;
    a.vendorMap   = v.map;
    a.vx = v.x; a.vy = v.y; a.vz = v.z;
    a.state       = BuyState::Travel;
    a.elapsedMs   = 0;
    a.timeoutMs   = BotAgentConfig::Instance().BuyTravelTimeout * 1000;
    a.savedStrategies = SuspendAI(botAI);

    bot->GetMotionMaster()->MovePoint(MOVE_POINT_ID, a.vx, a.vy, a.vz);

    g_busyBots.insert(botGuidLow);
    g_active.push_back(std::move(a));

    return std::string(R"({"accepted":true,"request_id":")") + BotAgentJson::Escape(g_active.back().requestId)
         + R"(","vendor_entry":)" + std::to_string(v.vendorEntry) + "}";
}

void BotAgentBuyAction::Tick(uint32 diffMs)
{
    bool const pauseInCombat = BotAgentConfig::Instance().BuyPauseInCombat;

    for (size_t i = 0; i < g_active.size();)
    {
        ActiveBuy& a = g_active[i];
        Player* bot = BotAgent::FindBotPlayer(a.botGuid);

        // Bot vanished (logged out / removed) — cannot restore AI; report failure.
        if (!bot)
        {
            Finalize(a, nullptr, nullptr, false, a.itemName, 0, "", "bot_left_world");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

        if (!bot->IsAlive())
        {
            Finalize(a, bot, botAI, false, a.itemName, 0, "", "bot_died");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        // While fighting, let the combat AI take over; don't advance or time out.
        if (bot->IsInCombat() && pauseInCombat)
        {
            ++i;
            continue;
        }

        a.elapsedMs += diffMs;
        if (a.elapsedMs > a.timeoutMs)
        {
            Finalize(a, bot, botAI, false, a.itemName, 0, "", "travel_timeout");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        if (a.state == BuyState::Travel)
        {
            float dist = bot->GetExactDist(a.vx, a.vy, a.vz);
            if (dist <= ARRIVE_DISTANCE)
            {
                a.state = BuyState::Buy;
            }
            else if (!bot->isMoving())
            {
                // Movement was cleared (e.g. by a brief interruption) — resume.
                bot->GetMotionMaster()->MovePoint(MOVE_POINT_ID, a.vx, a.vy, a.vz);
            }
            ++i;
            continue;
        }

        // --- BuyState::Buy ----------------------------------------------------
        Creature* vendor = bot->FindNearestCreature(a.vendorEntry, VENDOR_FIND_RANGE);
        if (!vendor || !bot->GetNPCIfCanInteractWith(vendor->GetGUID(), UNIT_NPC_FLAG_VENDOR))
        {
            Finalize(a, bot, botAI, false, a.itemName, 0, "", "vendor_unreachable");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        VendorItemData const* items = vendor->GetVendorItems();
        uint32 vendorSlot = 0;
        bool found = false;
        if (items)
        {
            for (uint32 s = 0; s < items->GetItemCount(); ++s)
            {
                if (items->GetItem(s)->item == a.itemEntry)
                {
                    vendorSlot = s;
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            Finalize(a, bot, botAI, false, a.itemName, 0, vendor->GetName(), "item_not_sold_here");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        uint32 moneyBefore = bot->GetMoney();
        uint32 countBefore = bot->GetItemCount(a.itemEntry, false);

        bot->BuyItemFromVendorSlot(vendor->GetGUID(), vendorSlot, a.itemEntry, 1, NULL_BAG, NULL_SLOT);

        uint32 countAfter = bot->GetItemCount(a.itemEntry, false);
        bool success = countAfter > countBefore;
        uint32 price = (success && moneyBefore >= bot->GetMoney()) ? (moneyBefore - bot->GetMoney()) : 0;

        Finalize(a, bot, botAI, success, a.itemName, price, vendor->GetName(),
                 success ? "bought" : "purchase_failed");
        g_active.erase(g_active.begin() + i);
    }
}
