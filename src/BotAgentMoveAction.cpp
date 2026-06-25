#include "BotAgentMoveAction.h"
#include "BotAgentActionCommon.h"
#include "BotAgentBots.h"
#include "BotAgentConfig.h"

#include "MotionMaster.h"
#include "Player.h"
#include "SharedDefines.h"

// mod-playerbots — bot AI control + master command handling.
#include "Playerbots.h"
#include "PlayerbotAI.h"

#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

using namespace BotAgentActionCommon;

namespace
{
    constexpr uint32 MOVE_POINT_ID    = 0xC0BE;  // "COME"
    constexpr float  ARRIVE_DISTANCE  = 4.0f;
    constexpr float  REPATH_DISTANCE  = 3.0f;

    struct ActiveCome
    {
        uint32                   botGuid    = 0;
        uint32                   playerGuid = 0;
        float                    tx = 0, ty = 0, tz = 0;
        std::vector<std::string> savedStrategies;
        uint32                   elapsedMs  = 0;
        uint32                   timeoutMs  = 120000;
    };

    std::vector<ActiveCome>     g_active;
    std::unordered_set<uint32>  g_comeBusy;

    void Finish(ActiveCome const& a, Player* bot, PlayerbotAI* botAI)
    {
        if (bot && botAI)
            RestoreAI(botAI, a.savedStrategies);
        g_comeBusy.erase(a.botGuid);
    }

    // Issue a built-in playerbot master command (e.g. "follow", "stay") as if
    // the requesting player had whispered it. Reuses all of playerbots' command
    // handling. Returns a small JSON ack.
    std::string RunBotCommand(uint32 botGuidLow, uint32 playerGuidLow, char const* command)
    {
        Player* bot = BotAgent::FindBotPlayer(botGuidLow);
        if (!bot)
            return R"({"ok":false,"reason":"bot_not_in_world"})";

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (!botAI)
            return R"({"ok":false,"reason":"not_a_bot"})";

        Player* from = BotAgent::FindBotPlayer(playerGuidLow);
        if (!from)
            return R"({"ok":false,"reason":"player_not_in_world"})";

        botAI->HandleCommand(CHAT_MSG_WHISPER, command, from);
        return R"({"ok":true})";
    }
}

std::string BotAgentMoveAction::Come(uint32 botGuidLow, uint32 playerGuidLow)
{
    if (g_comeBusy.count(botGuidLow))
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

    ActiveCome a;
    a.botGuid    = botGuidLow;
    a.playerGuid = playerGuidLow;
    a.tx = target->GetPositionX();
    a.ty = target->GetPositionY();
    a.tz = target->GetPositionZ();
    a.elapsedMs  = 0;
    a.timeoutMs  = BotAgentConfig::Instance().BuyTravelTimeout * 1000;
    a.savedStrategies = SuspendAI(botAI);

    bot->GetMotionMaster()->MovePoint(MOVE_POINT_ID, a.tx, a.ty, a.tz);

    g_comeBusy.insert(botGuidLow);
    g_active.push_back(std::move(a));

    return R"({"accepted":true})";
}

std::string BotAgentMoveAction::Follow(uint32 botGuidLow, uint32 playerGuidLow)
{
    return RunBotCommand(botGuidLow, playerGuidLow, "follow");
}

std::string BotAgentMoveAction::Stay(uint32 botGuidLow, uint32 playerGuidLow)
{
    return RunBotCommand(botGuidLow, playerGuidLow, "stay");
}

void BotAgentMoveAction::Tick(uint32 diffMs)
{
    bool const pauseInCombat = BotAgentConfig::Instance().BuyPauseInCombat;

    for (size_t i = 0; i < g_active.size();)
    {
        ActiveCome& a = g_active[i];
        Player* bot = BotAgent::FindBotPlayer(a.botGuid);

        if (!bot)
        {
            Finish(a, nullptr, nullptr);
            g_active.erase(g_active.begin() + i);
            continue;
        }

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

        if (!bot->IsAlive())
        {
            Finish(a, bot, botAI);
            g_active.erase(g_active.begin() + i);
            continue;
        }

        Player* target = BotAgent::FindBotPlayer(a.playerGuid);
        if (!target || target->GetMapId() != bot->GetMapId())
        {
            Finish(a, bot, botAI);
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
            Finish(a, bot, botAI);
            g_active.erase(g_active.begin() + i);
            continue;
        }

        float dist = bot->GetExactDist(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
        if (dist <= ARRIVE_DISTANCE)
        {
            Finish(a, bot, botAI);
            g_active.erase(g_active.begin() + i);
            continue;
        }

        // Re-path toward the player if they have moved or the bot stopped.
        float drift = std::sqrt((target->GetPositionX() - a.tx) * (target->GetPositionX() - a.tx)
                              + (target->GetPositionY() - a.ty) * (target->GetPositionY() - a.ty)
                              + (target->GetPositionZ() - a.tz) * (target->GetPositionZ() - a.tz));
        if (!bot->isMoving() || drift > REPATH_DISTANCE)
        {
            a.tx = target->GetPositionX();
            a.ty = target->GetPositionY();
            a.tz = target->GetPositionZ();
            bot->GetMotionMaster()->MovePoint(MOVE_POINT_ID, a.tx, a.ty, a.tz);
        }
        ++i;
    }
}
