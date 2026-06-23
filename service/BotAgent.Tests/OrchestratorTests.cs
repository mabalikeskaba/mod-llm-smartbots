using BotAgent.Service;
using BotAgent.Service.Llm;
using BotAgent.Service.Models;
using BotAgent.Service.Tools;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;
using Xunit;

namespace BotAgent.Tests;

public class OrchestratorTests
{
    private static AgentOrchestrator Make(ILlmProvider llm, FakeModuleClient module) =>
        new(llm, new ToolDispatcher(module, new PendingActions()), new ToolCatalog(), module,
            Options.Create(new LlmOptions()), NullLogger<AgentOrchestrator>.Instance);

    [Fact]
    public async Task Runs_tool_then_posts_final_ack_to_party()
    {
        var module = new FakeModuleClient { GoldResult = "{\"copper\":10000}" };
        var llm = new FakeLlmProvider(
            new LlmTurn(null,
                new[] { Build.Call("get_gold", "{\"bot_name\":\"Thrall\"}") },
                LlmStop.ToolUse),
            new LlmTurn("Thrall is carrying a single gold coin.",
                Array.Empty<ToolCallRequest>(), LlmStop.EndTurn));

        AgentOrchestrator orch = Make(llm, module);
        await orch.HandleAsync(Build.Request(Build.Member("Thrall", 42)), CancellationToken.None);

        Assert.Equal(2, llm.CallCount);                       // tool turn + final turn
        Assert.Contains("gold:42", module.Calls);             // tool executed
        Assert.Equal("12345", module.LastChatGroup);          // ack to the right group
        Assert.Equal("Thrall is carrying a single gold coin.", module.LastChatText);
    }

    [Fact]
    public async Task No_tools_just_acks()
    {
        var module = new FakeModuleClient();
        var llm = new FakeLlmProvider(
            new LlmTurn("Aye?", Array.Empty<ToolCallRequest>(), LlmStop.EndTurn));

        AgentOrchestrator orch = Make(llm, module);
        await orch.HandleAsync(Build.Request(Build.Member("Thrall", 42)), CancellationToken.None);

        Assert.Equal("Aye?", module.LastChatText);
        Assert.DoesNotContain(module.Calls, c => c.StartsWith("gold"));
    }

    [Fact]
    public async Task Empty_final_text_sends_no_chat()
    {
        var module = new FakeModuleClient();
        var llm = new FakeLlmProvider(
            new LlmTurn("   ", Array.Empty<ToolCallRequest>(), LlmStop.EndTurn));

        AgentOrchestrator orch = Make(llm, module);
        await orch.HandleAsync(Build.Request(Build.Member("Thrall", 42)), CancellationToken.None);

        Assert.DoesNotContain("chat", module.Calls);
    }
}
