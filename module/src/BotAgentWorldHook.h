#ifndef MOD_BOT_AGENT_WORLD_HOOK_H
#define MOD_BOT_AGENT_WORLD_HOOK_H

#include "ScriptMgr.h"

// WorldScript that owns the module lifecycle on the world thread:
//   - loads config and starts the HTTP server on startup,
//   - drains the cross-thread task queue (and, later, ticks active actions)
//     every world update,
//   - stops the HTTP server on shutdown.
class BotAgentWorldScript : public WorldScript
{
public:
    BotAgentWorldScript();

    void OnStartup() override;
    void OnShutdown() override;
    void OnAfterConfigLoad(bool reload) override;
    void OnUpdate(uint32 diff) override;
};

#endif // MOD_BOT_AGENT_WORLD_HOOK_H
