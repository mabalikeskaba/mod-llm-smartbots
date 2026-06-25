#include "BotAgentSellAction.h"
#include "BotAgentActionCommon.h"
#include "BotAgentBots.h"
#include "BotAgentConfig.h"
#include "BotAgentVendorIndex.h"

#include "Creature.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "MotionMaster.h"
#include "Opcodes.h"
#include "Player.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include "ItemPackets.h"

// mod-playerbots — bot AI control.
#include "Playerbots.h"
#include "PlayerbotAI.h"

#include <string>
#include <unordered_set>
#include <vector>

using namespace BotAgentActionCommon;

namespace
{
    constexpr uint32 MOVE_POINT_ID     = 0x5E11;  // "SELL"
    constexpr float  ARRIVE_DISTANCE   = 5.0f;
    constexpr float  VENDOR_FIND_RANGE = 10.0f;

    enum class SellState { Travel, Sell };

    struct ActiveSell
    {
        std::string                 requestId;
        uint32                      botGuid     = 0;
        std::unordered_set<uint32>  itemEntries;          // resolved items to sell
        std::string                 itemSummary;          // human-readable names
        uint32                      vendorEntry = 0;
        float                       vx = 0, vy = 0, vz = 0;
        std::vector<std::string>    savedStrategies;
        SellState                   state       = SellState::Travel;
        uint32                      elapsedMs   = 0;
        uint32                      timeoutMs   = 120000;
    };

    std::vector<ActiveSell>     g_active;
    std::unordered_set<uint32>  g_sellBusy;   // bots currently running a sell
    uint64                      g_counter = 0;

    std::unordered_set<uint32>& BusyBots() { return g_sellBusy; }

    std::string NextRequestId() { return "sell-" + std::to_string(++g_counter); }

    void Finalize(ActiveSell const& a, Player* bot, PlayerbotAI* botAI,
                  bool success, uint32 gainedCopper, std::string const& vendor,
                  std::string const& message)
    {
        if (bot && botAI)
            RestoreAI(botAI, a.savedStrategies);

        PostResult(a.requestId, success, a.itemSummary, gainedCopper, vendor, message);
        BusyBots().erase(a.botGuid);
    }
}

std::string BotAgentSellAction::Start(uint32 botGuidLow, std::string const& itemList, uint32 radiusYards)
{
    if (BusyBots().count(botGuidLow))
        return R"({"accepted":false,"reason":"busy"})";

    Player* bot = BotAgent::FindBotPlayer(botGuidLow);
    if (!bot)
        return R"({"accepted":false,"reason":"bot_not_in_world"})";

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return R"({"accepted":false,"reason":"not_a_bot"})";

    // Resolve the requested names and keep only items the bot actually carries.
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
        return R"({"accepted":false,"reason":"nothing_to_sell"})";

    std::vector<BotAgentVendorIndex::VendorCandidate> vendors =
        BotAgentVendorIndex::FindNearestAnyVendor(
            static_cast<uint16>(bot->GetMapId()),
            bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
            radiusYards, 25);

    if (vendors.empty())
        return R"({"accepted":false,"reason":"no_vendor_on_map"})";

    BotAgentVendorIndex::VendorCandidate const& v = vendors.front();

    ActiveSell a;
    a.requestId   = NextRequestId();
    a.botGuid     = botGuidLow;
    a.itemEntries = std::move(entries);
    a.itemSummary = summary;
    a.vendorEntry = v.vendorEntry;
    a.vx = v.x; a.vy = v.y; a.vz = v.z;
    a.state       = SellState::Travel;
    a.elapsedMs   = 0;
    a.timeoutMs   = BotAgentConfig::Instance().BuyTravelTimeout * 1000;
    a.savedStrategies = SuspendAI(botAI);

    bot->GetMotionMaster()->MovePoint(MOVE_POINT_ID, a.vx, a.vy, a.vz);

    BusyBots().insert(botGuidLow);
    g_active.push_back(std::move(a));

    return std::string(R"({"accepted":true,"request_id":")") + BotAgentJson::Escape(g_active.back().requestId)
         + R"(","vendor_entry":)" + std::to_string(v.vendorEntry) + "}";
}

void BotAgentSellAction::Tick(uint32 diffMs)
{
    bool const pauseInCombat = BotAgentConfig::Instance().BuyPauseInCombat;

    for (size_t i = 0; i < g_active.size();)
    {
        ActiveSell& a = g_active[i];
        Player* bot = BotAgent::FindBotPlayer(a.botGuid);

        if (!bot)
        {
            Finalize(a, nullptr, nullptr, false, 0, "", "bot_left_world");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

        if (!bot->IsAlive())
        {
            Finalize(a, bot, botAI, false, 0, "", "bot_died");
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
            Finalize(a, bot, botAI, false, 0, "", "travel_timeout");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        if (a.state == SellState::Travel)
        {
            float dist = bot->GetExactDist(a.vx, a.vy, a.vz);
            if (dist <= ARRIVE_DISTANCE)
                a.state = SellState::Sell;
            else if (!bot->isMoving())
                bot->GetMotionMaster()->MovePoint(MOVE_POINT_ID, a.vx, a.vy, a.vz);
            ++i;
            continue;
        }

        // --- SellState::Sell --------------------------------------------------
        Creature* vendor = bot->FindNearestCreature(a.vendorEntry, VENDOR_FIND_RANGE);
        if (!vendor || !bot->GetNPCIfCanInteractWith(vendor->GetGUID(), UNIT_NPC_FLAG_VENDOR))
        {
            Finalize(a, bot, botAI, false, 0, "", "vendor_unreachable");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        uint32 moneyBefore = bot->GetMoney();
        std::vector<Item*> items = CollectItemsByEntry(bot, a.itemEntries);
        uint32 soldStacks = 0;

        for (Item* item : items)
        {
            ObjectGuid itemGuid = item->GetGUID();
            uint32 count = item->GetCount();

            WorldPacket p(CMSG_SELL_ITEM);
            p << vendor->GetGUID() << itemGuid << count;

            WorldPackets::Item::SellItem packet(std::move(p));
            packet.Read();
            bot->GetSession()->HandleSellItemOpcode(packet);
            ++soldStacks;
        }

        uint32 gained = bot->GetMoney() > moneyBefore ? (bot->GetMoney() - moneyBefore) : 0;
        bool success = soldStacks > 0;

        Finalize(a, bot, botAI, success, gained, vendor->GetName(),
                 success ? "sold" : "nothing_sold");
        g_active.erase(g_active.begin() + i);
    }
}
