#include "BotAgentTradeAction.h"
#include "BotAgentActionCommon.h"
#include "BotAgentBots.h"
#include "BotAgentConfig.h"

#include "Item.h"
#include "ItemTemplate.h"
#include "MotionMaster.h"
#include "Opcodes.h"
#include "Player.h"
#include "TradeData.h"
#include "WorldPacket.h"
#include "WorldSession.h"

// mod-playerbots — bot AI control.
#include "Playerbots.h"
#include "PlayerbotAI.h"

#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

using namespace BotAgentActionCommon;

namespace
{
    constexpr uint32 MOVE_POINT_ID   = 0x71AD;  // "TrAD"
    constexpr float  TRADE_REACH  = 5.0f;    // comfortably inside trade range
    constexpr float  REPATH_DISTANCE = 3.0f;    // re-path if the player drifts this far

    enum class TradeState { Travel, Trade };

    struct ActiveTrade
    {
        std::string                 requestId;
        uint32                      botGuid    = 0;
        uint32                      playerGuid = 0;
        std::unordered_set<uint32>  itemEntries;
        std::string                 itemSummary;
        float                       tx = 0, ty = 0, tz = 0;   // last path target
        std::vector<std::string>    savedStrategies;
        TradeState                  state      = TradeState::Travel;
        uint32                      elapsedMs  = 0;
        uint32                      timeoutMs  = 120000;
    };

    std::vector<ActiveTrade>    g_active;
    std::unordered_set<uint32>  g_tradeBusy;
    uint64                      g_counter = 0;

    std::string NextRequestId() { return "trade-" + std::to_string(++g_counter); }

    void Finalize(ActiveTrade const& a, Player* bot, PlayerbotAI* botAI,
                  bool success, std::string const& message)
    {
        if (bot && botAI)
            RestoreAI(botAI, a.savedStrategies);

        PostResult(a.requestId, success, a.itemSummary, 0, "", message);
        g_tradeBusy.erase(a.botGuid);
    }

    // Place the carried items into the bot's open trade window (slots 0..5).
    // Returns the number of stacks placed. Assumes bot->GetTradeData() exists.
    uint32 PlaceItems(Player* bot, std::unordered_set<uint32> const& entries)
    {
        std::vector<Item*> items = CollectItemsByEntry(bot, entries);
        uint32 placed = 0;

        for (Item* item : items)
        {
            if (placed >= TRADE_SLOT_TRADED_COUNT)
                break;
            if (!item->CanBeTraded())
                continue;

            WorldPacket packet(CMSG_SET_TRADE_ITEM, 3);
            packet << static_cast<uint8>(placed);          // trade slot
            packet << static_cast<uint8>(item->GetBagSlot());
            packet << static_cast<uint8>(item->GetSlot());
            bot->GetSession()->HandleSetTradeItemOpcode(packet);
            ++placed;
        }
        return placed;
    }
}

std::string BotAgentTradeAction::Start(uint32 botGuidLow, uint32 playerGuidLow, std::string const& itemList)
{
    if (g_tradeBusy.count(botGuidLow))
        return R"({"accepted":false,"reason":"busy"})";

    Player* bot = BotAgent::FindBotPlayer(botGuidLow);
    if (!bot)
        return R"({"accepted":false,"reason":"bot_not_in_world"})";

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return R"({"accepted":false,"reason":"not_a_bot"})";

    Player* target = BotAgent::FindBotPlayer(playerGuidLow);
    if (!target)
        return R"({"accepted":false,"reason":"player_not_in_world"})";

    if (target->GetMapId() != bot->GetMapId())
        return R"({"accepted":false,"reason":"player_on_other_map"})";

    std::vector<std::string> names = SplitItemList(itemList);
    if (names.empty())
        return R"({"accepted":false,"reason":"no_items_named"})";

    std::unordered_set<uint32> entries;
    std::string summary;
    for (std::string const& n : names)
    {
        ResolvedItem ri = ResolveItemByName(n);
        if (!ri.entry)
            continue;
        if (bot->GetItemCount(ri.entry, false) == 0)
            continue;
        if (entries.insert(ri.entry).second)
        {
            if (!summary.empty())
                summary += ", ";
            summary += ri.name;
        }
    }

    if (entries.empty())
        return R"({"accepted":false,"reason":"nothing_to_give"})";

    ActiveTrade a;
    a.requestId   = NextRequestId();
    a.botGuid     = botGuidLow;
    a.playerGuid  = playerGuidLow;
    a.itemEntries = std::move(entries);
    a.itemSummary = summary;
    a.tx = target->GetPositionX();
    a.ty = target->GetPositionY();
    a.tz = target->GetPositionZ();
    a.state       = TradeState::Travel;
    a.elapsedMs   = 0;
    a.timeoutMs   = BotAgentConfig::Instance().BuyTravelTimeout * 1000;
    a.savedStrategies = SuspendAI(botAI);

    bot->GetMotionMaster()->MovePoint(MOVE_POINT_ID, a.tx, a.ty, a.tz);

    g_tradeBusy.insert(botGuidLow);
    g_active.push_back(std::move(a));

    return std::string(R"({"accepted":true,"request_id":")") + BotAgentJson::Escape(g_active.back().requestId) + "\"}";
}

void BotAgentTradeAction::Tick(uint32 diffMs)
{
    bool const pauseInCombat = BotAgentConfig::Instance().BuyPauseInCombat;

    for (size_t i = 0; i < g_active.size();)
    {
        ActiveTrade& a = g_active[i];
        Player* bot = BotAgent::FindBotPlayer(a.botGuid);

        if (!bot)
        {
            Finalize(a, nullptr, nullptr, false, "bot_left_world");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

        if (!bot->IsAlive())
        {
            Finalize(a, bot, botAI, false, "bot_died");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        Player* target = BotAgent::FindBotPlayer(a.playerGuid);
        if (!target || target->GetMapId() != bot->GetMapId())
        {
            Finalize(a, bot, botAI, false, "player_unreachable");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        if (bot->IsInCombat() && pauseInCombat)
        {
            ++i;
            continue;
        }

        a.elapsedMs += diffMs;
        if (a.elapsedMs > a.timeoutMs)
        {
            Finalize(a, bot, botAI, false, "travel_timeout");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        if (a.state == TradeState::Travel)
        {
            float dist = bot->GetExactDist(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
            if (dist <= TRADE_REACH)
            {
                a.state = TradeState::Trade;
                ++i;
                continue;
            }

            // Re-path if the player has drifted from the last target or the bot
            // stopped moving (e.g. a brief interruption).
            float drift = bot->GetExactDist(a.tx, a.ty, a.tz) > 0.0f
                ? std::sqrt((target->GetPositionX() - a.tx) * (target->GetPositionX() - a.tx)
                          + (target->GetPositionY() - a.ty) * (target->GetPositionY() - a.ty)
                          + (target->GetPositionZ() - a.tz) * (target->GetPositionZ() - a.tz))
                : 0.0f;

            if (!bot->isMoving() || drift > REPATH_DISTANCE)
            {
                a.tx = target->GetPositionX();
                a.ty = target->GetPositionY();
                a.tz = target->GetPositionZ();
                bot->GetMotionMaster()->MovePoint(MOVE_POINT_ID, a.tx, a.ty, a.tz);
            }
            ++i;
            continue;
        }

        // --- TradeState::Trade ------------------------------------------------
        // Open the trade with the player, place the items, and accept the bot's
        // side. The player confirms on their client to finish the exchange.
        WorldPacket initiate(CMSG_INITIATE_TRADE);
        initiate << target->GetGUID();
        bot->GetSession()->HandleInitiateTradeOpcode(initiate);

        if (!bot->GetTradeData())
        {
            Finalize(a, bot, botAI, false, "trade_init_failed");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        uint32 placed = PlaceItems(bot, a.itemEntries);
        if (placed == 0)
        {
            WorldPacket cancel;
            bot->GetSession()->HandleCancelTradeOpcode(cancel);
            Finalize(a, bot, botAI, false, "nothing_tradeable");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        WorldPacket accept;
        bot->GetSession()->HandleAcceptTradeOpcode(accept);

        Finalize(a, bot, botAI, true, "trade_opened_awaiting_player");
        g_active.erase(g_active.begin() + i);
    }
}
