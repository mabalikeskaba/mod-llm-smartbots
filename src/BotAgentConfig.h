#ifndef MOD_BOT_AGENT_CONFIG_H
#define MOD_BOT_AGENT_CONFIG_H

#include "Define.h"
#include <string>

// Holds the module's configuration, loaded from mod_bot_agent.conf via
// AzerothCore's sConfigMgr. Loaded once on startup (and on .reload config).
class BotAgentConfig
{
public:
    static BotAgentConfig& Instance();

    void Load();

    bool        Enable           = true;
    std::string BindAddress      = "127.0.0.1";
    uint16      Port             = 8810;
    std::string Token;
    std::string CommandPrefix    = "!";
    std::string IncomingUrl      = "http://127.0.0.1:8820/incoming";
    std::string CallbackBaseUrl  = "http://127.0.0.1:8820";
    uint32      BuyScanRadius    = 0;
    uint32      BuyTravelTimeout = 120;
    bool        BuyPauseInCombat = true;

private:
    BotAgentConfig() = default;
};

#endif // MOD_BOT_AGENT_CONFIG_H
