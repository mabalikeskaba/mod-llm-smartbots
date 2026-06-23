#include "BotAgentChatHook.h"
#include "BotAgentWorldHook.h"

// Entry point invoked by AzerothCore's generated module loader. The function
// name must match the module directory: mod-bot-agent -> Addmod_bot_agentScripts.
void Addmod_bot_agentScripts()
{
    new BotAgentWorldScript();
    new BotAgentChatHook();
}
