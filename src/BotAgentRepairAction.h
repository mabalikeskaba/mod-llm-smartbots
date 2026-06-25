#ifndef MOD_BOT_AGENT_REPAIR_ACTION_H
#define MOD_BOT_AGENT_REPAIR_ACTION_H

#include "Define.h"
#include <string>

// Asynchronous "travel to the nearest repair NPC and fix all gear" action.
// Mirrors the sell action: find the nearest repair-flagged NPC on the bot's
// map, walk there, repair weapons and all equipped/carried items, then report
// the total cost back to the C# service.
//
// All functions run on the WORLD THREAD only.
namespace BotAgentRepairAction
{
    // Returns initial JSON: {"accepted":true,"request_id":"..."} or
    // {"accepted":false,"reason":"..."}.
    std::string Start(uint32 botGuidLow, uint32 radiusYards);

    // Advance all in-flight repair actions by `diffMs`.
    void Tick(uint32 diffMs);
}

#endif // MOD_BOT_AGENT_REPAIR_ACTION_H
