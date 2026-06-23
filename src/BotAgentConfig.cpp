#include "BotAgentConfig.h"
#include "Config.h"
#include "Log.h"

BotAgentConfig& BotAgentConfig::Instance()
{
    static BotAgentConfig instance;
    return instance;
}

void BotAgentConfig::Load()
{
    Enable           = sConfigMgr->GetOption<bool>("LLMAgent.Enable", true);
    BindAddress      = sConfigMgr->GetOption<std::string>("LLMAgent.Http.BindAddress", "127.0.0.1");
    Port             = static_cast<uint16>(sConfigMgr->GetOption<uint32>("LLMAgent.Http.Port", 8810));
    Token            = sConfigMgr->GetOption<std::string>("LLMAgent.Http.Token", "");
    CommandPrefix    = sConfigMgr->GetOption<std::string>("LLMAgent.CommandPrefix", "!");
    IncomingUrl      = sConfigMgr->GetOption<std::string>("LLMAgent.Service.IncomingUrl", "http://127.0.0.1:8820/incoming");
    CallbackBaseUrl  = sConfigMgr->GetOption<std::string>("LLMAgent.Service.CallbackBaseUrl", "http://127.0.0.1:8820");
    BuyScanRadius    = sConfigMgr->GetOption<uint32>("LLMAgent.Buy.ScanRadius", 0);
    BuyTravelTimeout = sConfigMgr->GetOption<uint32>("LLMAgent.Buy.TravelTimeoutSeconds", 120);
    BuyPauseInCombat = sConfigMgr->GetOption<bool>("LLMAgent.Buy.PauseDuringCombat", true);

    if (Enable && Token.empty())
        LOG_WARN("module.bot_agent",
            "[bot-agent] LLMAgent.Http.Token is empty — the HTTP endpoint is unauthenticated. Set a shared secret.");
}
