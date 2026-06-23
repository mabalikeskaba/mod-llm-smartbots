#include "BotAgentTaskQueue.h"
#include "Log.h"

#include <chrono>
#include <future>
#include <memory>

BotAgentTaskQueue& BotAgentTaskQueue::Instance()
{
    static BotAgentTaskQueue instance;
    return instance;
}

void BotAgentTaskQueue::Enqueue(Task task)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _tasks.push(std::move(task));
}

void BotAgentTaskQueue::DrainOnWorldThread()
{
    // Move the pending tasks out under the lock, then run them unlocked so a
    // task that enqueues more work (or runs long) does not block producers.
    std::queue<Task> pending;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::swap(pending, _tasks);
    }

    while (!pending.empty())
    {
        Task& task = pending.front();
        try
        {
            task();
        }
        catch (std::exception const& ex)
        {
            LOG_ERROR("module.bot_agent", "[bot-agent] world task threw: {}", ex.what());
        }
        catch (...)
        {
            LOG_ERROR("module.bot_agent", "[bot-agent] world task threw unknown exception");
        }
        pending.pop();
    }
}

std::string BotAgentTaskQueue::RunSync(std::function<std::string()> fn, uint32 timeoutMs)
{
    auto promise = std::make_shared<std::promise<std::string>>();
    std::future<std::string> future = promise->get_future();

    Instance().Enqueue([promise, fn = std::move(fn)]()
    {
        std::string result;
        try
        {
            result = fn();
        }
        catch (...)
        {
            result = R"({"error":"internal_exception"})";
        }
        promise->set_value(std::move(result));
    });

    if (future.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready)
        return future.get();

    // The task may still run later on the world thread; its result is discarded.
    return R"({"error":"world_thread_timeout"})";
}
