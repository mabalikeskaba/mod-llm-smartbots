#ifndef MOD_BOT_AGENT_CHAT_H
#define MOD_BOT_AGENT_CHAT_H

#include "Define.h"
#include <string>

// Sends an acknowledgement line into a party/raid, authored by one of the
// group's bots. Must be called on the world thread. Returns a JSON result
// ({"sent":true} or an error object).
namespace BotAgentChat
{
    std::string SendPartyMessage(uint32 groupLowId, std::string const& text);
}

#endif // MOD_BOT_AGENT_CHAT_H
