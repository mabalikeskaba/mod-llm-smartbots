#include "BotAgentWorldHook.h"
#include "BotAgentBuyAction.h"
#include "BotAgentConfig.h"
#include "BotAgentHttpClient.h"
#include "BotAgentHttpServer.h"
#include "BotAgentTaskQueue.h"
#include "Log.h"

BotAgentWorldScript::BotAgentWorldScript()
    : WorldScript("BotAgentWorldScript")
{
}

void BotAgentWorldScript::OnStartup()
{
    BotAgentConfig& cfg = BotAgentConfig::Instance();
    cfg.Load();

    if (!cfg.Enable)
    {
        LOG_INFO("module.bot_agent", "[bot-agent] disabled via LLMAgent.Enable = 0");
        return;
    }

    BotAgentHttpClient::Instance().Start();
    BotAgentHttpServer::Instance().Start();
}

void BotAgentWorldScript::OnShutdown()
{
    BotAgentHttpServer::Instance().Stop();
    BotAgentHttpClient::Instance().Stop();
}

void BotAgentWorldScript::OnAfterConfigLoad(bool reload)
{
    if (!reload)
        return; // initial load is handled in OnStartup

    // Apply config changes: restart the server/client to pick up new
    // bind/token/url values.
    BotAgentConfig& cfg = BotAgentConfig::Instance();
    BotAgentHttpServer::Instance().Stop();
    BotAgentHttpClient::Instance().Stop();
    cfg.Load();
    if (cfg.Enable)
    {
        BotAgentHttpClient::Instance().Start();
        BotAgentHttpServer::Instance().Start();
    }
}

void BotAgentWorldScript::OnUpdate(uint32 diff)
{
    // Run everything the HTTP threads queued for the world thread.
    BotAgentTaskQueue::Instance().DrainOnWorldThread();

    // Advance in-flight async actions (vendor runs).
    BotAgentBuyAction::Tick(diff);
}
