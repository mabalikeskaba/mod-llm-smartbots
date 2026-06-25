#ifndef MOD_BOT_AGENT_SELL_ACTION_H
#define MOD_BOT_AGENT_SELL_ACTION_H

#include "Define.h"
#include <string>

// Asynchronous "sell these items at the nearest vendor to free bag space"
// action. The LLM decides which items to sell and passes their names; the
// module resolves them, travels to the nearest vendor on the bot's map, and
// sells every matching stack the bot is carrying.
//
// All functions run on the WORLD THREAD only (Start via the task queue, Tick
// from WorldScript::OnUpdate), so the in-flight list needs no locking.
namespace BotAgentSellAction
{
    // `itemList` is a '|'-delimited list of item names. Returns initial JSON:
    // {"accepted":true,"request_id":"...","vendor_entry":N} or
    // {"accepted":false,"reason":"..."}.
    std::string Start(uint32 botGuidLow, std::string const& itemList, uint32 radiusYards);

    // Advance all in-flight sell actions by `diffMs`.
    void Tick(uint32 diffMs);
}

#endif // MOD_BOT_AGENT_SELL_ACTION_H
