#ifndef MOD_BOT_AGENT_MOVE_ACTION_H
#define MOD_BOT_AGENT_MOVE_ACTION_H

#include "Define.h"
#include <string>

// Movement / positioning commands for a companion.
//
//   come   — asynchronous one-shot: walk to the requesting player's current
//            position, then restore the bot's normal AI. Ticked on the world
//            thread; no /action_result callback (the bot just arrives).
//   follow — persistent: hand "follow" to the playerbot AI (bot follows master).
//   stay   — persistent: hand "stay" to the playerbot AI (bot holds position).
//
// follow/stay are synchronous (an instant AI strategy change) and return their
// result immediately; come returns {"accepted":true,...} and finishes via Tick.
// All functions run on the WORLD THREAD only.
namespace BotAgentMoveAction
{
    std::string Come(uint32 botGuidLow, uint32 playerGuidLow);
    std::string Follow(uint32 botGuidLow, uint32 playerGuidLow);
    std::string Stay(uint32 botGuidLow, uint32 playerGuidLow);

    // Advance in-flight "come" moves by `diffMs`.
    void Tick(uint32 diffMs);
}

#endif // MOD_BOT_AGENT_MOVE_ACTION_H
