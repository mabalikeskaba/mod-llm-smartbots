using BotAgent.Service;
using BotAgent.Service.Llm;
using BotAgent.Service.Models;
using BotAgent.Service.Tools;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;
using Xunit;

namespace BotAgent.Tests;

public class MoneyTests
{
    [Theory]
    [InlineData(0, "0c")]
    [InlineData(55, "55c")]
    [InlineData(100, "1s")]
    [InlineData(15500, "1g 55s")]
    [InlineData(10000, "1g")]
    public void Formats_copper(long copper, string expected) =>
        Assert.Equal(expected, Money.Format(copper));
}

public class PendingActionsTests
{
    [Fact]
    public void Register_then_take_removes_entry()
    {
        var store = new PendingActions();
        store.Register(new PendingAction("r1", "g", "Hero", "Thrall", "Potion"));

        Assert.True(store.TryTake("r1", out PendingAction a));
        Assert.Equal("Thrall", a.BotName);
        Assert.False(store.TryTake("r1", out _));  // taken once
    }
}

public class BuyRegistersPendingTests
{
    [Fact]
    public async Task Accepted_buy_registers_pending_action()
    {
        var module = new FakeModuleClient
        {
            BuyResult = "{\"accepted\":true,\"request_id\":\"req-7\",\"vendor\":\"Innkeeper\"}"
        };
        var pending = new PendingActions();
        var dispatcher = new ToolDispatcher(module, pending);
        IncomingRequest req = Build.Request(Build.Member("Thrall", 42));

        await dispatcher.DispatchAsync(
            Build.Call("buy_item_from_vendor",
                "{\"bot_name\":\"Thrall\",\"item_name\":\"Healing Potion\"}"),
            req, CancellationToken.None);

        Assert.True(pending.TryTake("req-7", out PendingAction a));
        Assert.Equal("Healing Potion", a.ItemName);
        Assert.Equal("12345", a.GroupGuid);
    }

    [Fact]
    public async Task Rejected_buy_registers_nothing()
    {
        var module = new FakeModuleClient { BuyResult = "{\"accepted\":false,\"reason\":\"busy\"}" };
        var pending = new PendingActions();
        var dispatcher = new ToolDispatcher(module, pending);
        IncomingRequest req = Build.Request(Build.Member("Thrall", 42));

        await dispatcher.DispatchAsync(
            Build.Call("buy_item_from_vendor",
                "{\"bot_name\":\"Thrall\",\"item_name\":\"Potion\"}"),
            req, CancellationToken.None);

        Assert.Equal(0, pending.Count);
    }

    [Fact]
    public async Task Sell_joins_item_names_and_registers_pending_as_sell()
    {
        var module = new FakeModuleClient();
        var pending = new PendingActions();
        var dispatcher = new ToolDispatcher(module, pending);
        IncomingRequest req = Build.Request(Build.Member("Thrall", 42));

        await dispatcher.DispatchAsync(
            Build.Call("sell_items_to_vendor",
                "{\"bot_name\":\"Thrall\",\"item_names\":[\"Broken Fang\",\"Tattered Cloth\"]}"),
            req, CancellationToken.None);

        Assert.Equal(42u, module.LastSellGuid);
        Assert.Contains("Broken Fang|Tattered Cloth", module.LastSellBody);
        Assert.True(pending.TryTake("s1", out PendingAction a));
        Assert.Equal("sell", a.Kind);
        Assert.Equal("Broken Fang, Tattered Cloth", a.ItemName);
    }

    [Fact]
    public async Task Give_passes_player_guid_and_registers_pending_as_give()
    {
        var module = new FakeModuleClient();
        var pending = new PendingActions();
        var dispatcher = new ToolDispatcher(module, pending);
        IncomingRequest req = Build.Request(Build.Member("Thrall", 42));

        await dispatcher.DispatchAsync(
            Build.Call("give_items_to_player",
                "{\"bot_name\":\"Thrall\",\"item_names\":[\"Mild Spices\"]}"),
            req, CancellationToken.None);

        Assert.Equal(42u, module.LastTradeGuid);
        Assert.Contains("\"player_guid\":1", module.LastTradeBody);
        Assert.Contains("Mild Spices", module.LastTradeBody);
        Assert.True(pending.TryTake("t1", out PendingAction a));
        Assert.Equal("give", a.Kind);
    }

    [Fact]
    public async Task Repair_calls_module_and_registers_pending_as_repair()
    {
        var module = new FakeModuleClient();
        var pending = new PendingActions();
        var dispatcher = new ToolDispatcher(module, pending);
        IncomingRequest req = Build.Request(Build.Member("Thrall", 42));

        await dispatcher.DispatchAsync(
            Build.Call("repair_equipment", "{\"bot_name\":\"Thrall\"}"),
            req, CancellationToken.None);

        Assert.Equal(42u, module.LastRepairGuid);
        Assert.True(pending.TryTake("rp1", out PendingAction a));
        Assert.Equal("repair", a.Kind);
    }
}

public class ActionResultAckTests
{
    private static AgentOrchestrator Make(ILlmProvider llm, FakeModuleClient module) =>
        new(llm, new ToolDispatcher(module, new PendingActions()), new ToolCatalog(), module,
            Options.Create(new LlmOptions()), NullLogger<AgentOrchestrator>.Instance);

    [Fact]
    public async Task Posts_completion_ack_to_the_party()
    {
        var module = new FakeModuleClient();
        var llm = new FakeLlmProvider(
            new LlmTurn("Got your healing potion, Hero.", Array.Empty<ToolCallRequest>(), LlmStop.EndTurn));
        AgentOrchestrator orch = Make(llm, module);

        var ctx = new PendingAction("req-7", "12345", "Hero", "Thrall", "Healing Potion");
        var result = new ActionResult("req-7", true, "Healing Potion", 2550, "Innkeeper Bates", null);

        await orch.CompleteActionAsync(result, ctx, CancellationToken.None);

        Assert.Equal("12345", module.LastChatGroup);
        Assert.Equal("Got your healing potion, Hero.", module.LastChatText);
    }
}
