#include "BotAgentWorldHook.h"

// Entry point invoked by AzerothCore's generated module loader. The function
// name must match the module directory: mod-bot-agent -> Addmod_bot_agentScripts.
void Addmod_bot_agentScripts()
{
    new BotAgentWorldScript();
    // Chat hook and read/action scripts are registered by later units.
}
