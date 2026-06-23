#ifndef MOD_BOT_AGENT_HTTP_CLIENT_H
#define MOD_BOT_AGENT_HTTP_CLIENT_H

#include "Define.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>

// Fire-and-forget outbound HTTP for the module: the chat hook (world thread)
// and the buy-action (world thread) must never block on a network call, so
// requests are queued here and sent by a single background worker thread.
//
// Used for:
//   - POST <IncomingUrl>            (player command forwarded to the service)
//   - POST <CallbackBaseUrl>/action_result (async action result)
class BotAgentHttpClient
{
public:
    static BotAgentHttpClient& Instance();

    void Start();
    void Stop();

    // Enqueue a JSON POST to an absolute URL. Returns immediately. The shared
    // X-Agent-Token header is attached automatically.
    void PostJson(std::string url, std::string jsonBody);

private:
    BotAgentHttpClient() = default;

    void WorkerLoop();
    void Send(std::string const& url, std::string const& body);

    std::thread                                     _worker;
    std::mutex                                      _mutex;
    std::condition_variable                         _cv;
    std::queue<std::pair<std::string, std::string>> _jobs; // (url, body)
    std::atomic<bool>                               _running{false};
};

#endif // MOD_BOT_AGENT_HTTP_CLIENT_H
