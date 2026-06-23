#ifndef MOD_BOT_AGENT_HTTP_SERVER_H
#define MOD_BOT_AGENT_HTTP_SERVER_H

#include "Define.h"
#include <atomic>
#include <memory>
#include <thread>

// httplib is only included in the .cpp to keep its (winsock-heavy) headers out
// of the rest of the module and away from AzerothCore's networking code.
namespace httplib { class Server; }

// Runs the module's HTTP server on its own thread. Routes never touch game
// objects directly — they marshal onto the world thread via BotAgentTaskQueue.
class BotAgentHttpServer
{
public:
    static BotAgentHttpServer& Instance();

    void Start();   // idempotent; spawns the listener thread
    void Stop();     // idempotent; stops the listener and joins the thread

    ~BotAgentHttpServer();

private:
    BotAgentHttpServer() = default;

    void RegisterRoutes();

    std::unique_ptr<httplib::Server> _server;
    std::thread                      _thread;
    std::atomic<bool>                _running{false};
};

#endif // MOD_BOT_AGENT_HTTP_SERVER_H
