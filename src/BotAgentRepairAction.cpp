#include "BotAgentRepairAction.h"
#include "BotAgentActionCommon.h"
#include "BotAgentBots.h"
#include "BotAgentConfig.h"
#include "BotAgentVendorIndex.h"

#include "Creature.h"
#include "MotionMaster.h"
#include "Player.h"
#include "SharedDefines.h"

// mod-playerbots — bot AI control.
#include "Playerbots.h"
#include "PlayerbotAI.h"

#include <string>
#include <unordered_set>
#include <vector>

using namespace BotAgentActionCommon;

namespace
{
    constexpr uint32 MOVE_POINT_ID     = 0x5EBA;  // "REPA"
    constexpr float  ARRIVE_DISTANCE   = 5.0f;
    constexpr float  NPC_FIND_RANGE    = 10.0f;
    constexpr uint32 NPC_FLAG_REPAIR   = 0x1000;  // UNIT_NPC_FLAG_REPAIR

    enum class RepairState { Travel, Repair };

    struct ActiveRepair
    {
        std::string                 requestId;
        uint32                      botGuid     = 0;
        uint32                      npcEntry    = 0;
        float                       vx = 0, vy = 0, vz = 0;
        std::vector<std::string>    savedStrategies;
        RepairState                 state       = RepairState::Travel;
        uint32                      elapsedMs   = 0;
        uint32                      timeoutMs   = 120000;
    };

    std::vector<ActiveRepair>   g_active;
    std::unordered_set<uint32>  g_repairBusy;
    uint64                      g_counter = 0;

    std::string NextRequestId() { return "repair-" + std::to_string(++g_counter); }

    void Finalize(ActiveRepair const& a, Player* bot, PlayerbotAI* botAI,
                  bool success, uint32 costCopper, std::string const& npc,
                  std::string const& message)
    {
        if (bot && botAI)
            RestoreAI(botAI, a.savedStrategies);

        // item = "" (no item subject), price = repair cost, vendor = NPC name.
        PostResult(a.requestId, success, "", costCopper, npc, message);
        g_repairBusy.erase(a.botGuid);
    }
}

std::string BotAgentRepairAction::Start(uint32 botGuidLow, uint32 radiusYards)
{
    if (g_repairBusy.count(botGuidLow))
        return R"({"accepted":false,"reason":"busy"})";

    Player* bot = BotAgent::FindBotPlayer(botGuidLow);
    if (!bot)
        return R"({"accepted":false,"reason":"bot_not_in_world"})";

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return R"({"accepted":false,"reason":"not_a_bot"})";

    std::vector<BotAgentVendorIndex::VendorCandidate> npcs =
        BotAgentVendorIndex::FindNearestNpcWithFlag(
            static_cast<uint16>(bot->GetMapId()),
            bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
            NPC_FLAG_REPAIR, radiusYards, 25);

    if (npcs.empty())
        return R"({"accepted":false,"reason":"no_repair_npc_on_map"})";

    BotAgentVendorIndex::VendorCandidate const& v = npcs.front();

    ActiveRepair a;
    a.requestId   = NextRequestId();
    a.botGuid     = botGuidLow;
    a.npcEntry    = v.vendorEntry;
    a.vx = v.x; a.vy = v.y; a.vz = v.z;
    a.state       = RepairState::Travel;
    a.elapsedMs   = 0;
    a.timeoutMs   = BotAgentConfig::Instance().BuyTravelTimeout * 1000;
    a.savedStrategies = SuspendAI(botAI);

    bot->GetMotionMaster()->MovePoint(MOVE_POINT_ID, a.vx, a.vy, a.vz);

    g_repairBusy.insert(botGuidLow);
    g_active.push_back(std::move(a));

    return std::string(R"({"accepted":true,"request_id":")") + BotAgentJson::Escape(g_active.back().requestId) + "\"}";
}

void BotAgentRepairAction::Tick(uint32 diffMs)
{
    bool const pauseInCombat = BotAgentConfig::Instance().BuyPauseInCombat;

    for (size_t i = 0; i < g_active.size();)
    {
        ActiveRepair& a = g_active[i];
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

        if (a.state == RepairState::Travel)
        {
            float dist = bot->GetExactDist(a.vx, a.vy, a.vz);
            if (dist <= ARRIVE_DISTANCE)
                a.state = RepairState::Repair;
            else if (!bot->isMoving())
                bot->GetMotionMaster()->MovePoint(MOVE_POINT_ID, a.vx, a.vy, a.vz);
            ++i;
            continue;
        }

        // --- RepairState::Repair ----------------------------------------------
        Creature* npc = bot->FindNearestCreature(a.npcEntry, NPC_FIND_RANGE);
        if (!npc || !bot->GetNPCIfCanInteractWith(npc->GetGUID(), UNIT_NPC_FLAG_REPAIR))
        {
            Finalize(a, bot, botAI, false, 0, "", "repair_npc_unreachable");
            g_active.erase(g_active.begin() + i);
            continue;
        }

        bot->SetFacingToObject(npc);
        float discountMod = bot->GetReputationPriceDiscount(npc);

        // Weapons/ranged/offhand first, then everything else.
        uint32 cost = bot->DurabilityRepair(EQUIPMENT_SLOT_MAINHAND, true, discountMod, false);
        cost += bot->DurabilityRepair(EQUIPMENT_SLOT_RANGED, true, discountMod, false);
        cost += bot->DurabilityRepair(EQUIPMENT_SLOT_OFFHAND, true, discountMod, false);
        cost += bot->DurabilityRepairAll(true, discountMod, false);

        Finalize(a, bot, botAI, true, cost, npc->GetName(), cost > 0 ? "repaired" : "nothing_to_repair");
        g_active.erase(g_active.begin() + i);
    }
}
