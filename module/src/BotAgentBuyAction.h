#ifndef MOD_BOT_AGENT_BUY_ACTION_H
#define MOD_BOT_AGENT_BUY_ACTION_H

#include "Define.h"
#include <string>

// Asynchronous "buy item X from the nearest vendor on the bot's map" action.
//
// All functions run on the WORLD THREAD only (Start via the task queue, Tick
// from WorldScript::OnUpdate), so the in-flight action list needs no locking.
//
// Flow:
//   Start(): validate bot/item/vendor, suspend the bot's playerbot AI, begin
//            travelling, and return the initial JSON {accepted, request_id, ...}.
//   Tick():  advance each in-flight action (travel -> buy), restore the AI and
//            POST the result to the C# service on completion/failure/timeout.
namespace BotAgentBuyAction
{
    // Returns initial JSON: {"accepted":true,"request_id":"...","vendor_entry":N}
    // or {"accepted":false,"reason":"..."}.
    std::string Start(uint32 botGuidLow, std::string const& itemName, uint32 radiusYards);

    // Advance all in-flight actions by `diffMs`.
    void Tick(uint32 diffMs);
}

#endif // MOD_BOT_AGENT_BUY_ACTION_H
