#ifndef MOD_BOT_AGENT_CHAT_HOOK_H
#define MOD_BOT_AGENT_CHAT_HOOK_H

#include "ScriptMgr.h"

// Forwards party/raid chat messages that start with the command prefix to the
// C# service, together with the player's group roster. Everything else is
// ignored. Runs on the world thread; the outbound POST is async.
class BotAgentChatHook : public PlayerScript
{
public:
    BotAgentChatHook();

    void OnPlayerBeforeSendChatMessage(
        Player* player, uint32& type, uint32& lang, std::string& msg) override;
};

#endif // MOD_BOT_AGENT_CHAT_HOOK_H
