#ifndef MOD_BOT_AGENT_TASK_QUEUE_H
#define MOD_BOT_AGENT_TASK_QUEUE_H

#include "Define.h"
#include <functional>
#include <mutex>
#include <queue>
#include <string>

// Bridges the HTTP server thread and the world-update thread.
//
// AzerothCore game objects (Player*, Creature*, ...) may only be touched from
// the world-update thread. HTTP handlers therefore never touch them directly:
// they enqueue a task here and (for synchronous reads) block on a future while
// the world thread runs the task and fulfils it.
class BotAgentTaskQueue
{
public:
    using Task = std::function<void()>;

    static BotAgentTaskQueue& Instance();

    // Called from any thread (typically the HTTP server thread).
    void Enqueue(Task task);

    // Called once per world tick from the world-update thread.
    void DrainOnWorldThread();

    // Helper for synchronous request/response: runs `fn` on the world thread and
    // waits up to `timeoutMs` for its JSON result. On timeout returns a JSON
    // error body. Safe to call from the HTTP thread; must NOT be called from the
    // world thread (it would deadlock).
    static std::string RunSync(std::function<std::string()> fn, uint32 timeoutMs);

private:
    BotAgentTaskQueue() = default;

    std::mutex        _mutex;
    std::queue<Task>  _tasks;
};

#endif // MOD_BOT_AGENT_TASK_QUEUE_H
