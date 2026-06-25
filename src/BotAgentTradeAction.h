#ifndef MOD_BOT_AGENT_TRADE_ACTION_H
#define MOD_BOT_AGENT_TRADE_ACTION_H

#include "Define.h"
#include <string>

// Asynchronous "walk back to the player and hand over these items via trade"
// action. The bot travels to the requesting player, opens a trade window, puts
// the named items it is carrying into the trade slots, and accepts its own
// side. The player confirms on their client to complete the trade.
//
// All functions run on the WORLD THREAD only (Start via the task queue, Tick
// from WorldScript::OnUpdate), so the in-flight list needs no locking.
namespace BotAgentTradeAction
{
    // `itemList` is a '|'-delimited list of item names. `playerGuidLow` is the
    // requesting player (trade target). Returns initial JSON:
    // {"accepted":true,"request_id":"..."} or {"accepted":false,"reason":"..."}.
    std::string Start(uint32 botGuidLow, uint32 playerGuidLow, std::string const& itemList);

    // Advance all in-flight trade actions by `diffMs`.
    void Tick(uint32 diffMs);
}

#endif // MOD_BOT_AGENT_TRADE_ACTION_H
