using BotAgent.Service;
using BotAgent.Service.Models;
using BotAgent.Service.Tools;
using Xunit;

namespace BotAgent.Tests;

public class ToolDispatcherTests
{
    [Fact]
    public async Task Get_gold_resolves_name_to_guid_and_returns_module_body()
    {
        var module = new FakeModuleClient { GoldResult = "{\"copper\":15500}" };
        var dispatcher = new ToolDispatcher(module, new PendingActions());
        IncomingRequest req = Build.Request(Build.Member("Thrall", 42));

        string result = await dispatcher.DispatchAsync(
            Build.Call("get_gold", "{\"bot_name\":\"Thrall\"}"), req, CancellationToken.None);

        Assert.Equal("{\"copper\":15500}", result);
        Assert.Contains("gold:42", module.Calls);
    }

    [Fact]
    public async Task Name_match_is_case_insensitive()
    {
        var module = new FakeModuleClient();
        var dispatcher = new ToolDispatcher(module, new PendingActions());
        IncomingRequest req = Build.Request(Build.Member("Thrall", 42));

        await dispatcher.DispatchAsync(
            Build.Call("get_level", "{\"bot_name\":\"thrall\"}"), req, CancellationToken.None);

        Assert.Contains("level:42", module.Calls);
    }

    [Fact]
    public async Task Unknown_bot_returns_error()
    {
        var dispatcher = new ToolDispatcher(new FakeModuleClient(), new PendingActions());
        IncomingRequest req = Build.Request(Build.Member("Thrall", 42));

        string result = await dispatcher.DispatchAsync(
            Build.Call("get_gold", "{\"bot_name\":\"Jaina\"}"), req, CancellationToken.None);

        Assert.Contains("no companion named", result);
    }

    [Fact]
    public async Task Buy_passes_item_name_and_radius_to_module()
    {
        var module = new FakeModuleClient();
        var dispatcher = new ToolDispatcher(module, new PendingActions());
        IncomingRequest req = Build.Request(Build.Member("Thrall", 42));

        await dispatcher.DispatchAsync(
            Build.Call("buy_item_from_vendor",
                "{\"bot_name\":\"Thrall\",\"item_name\":\"Healing Potion\",\"radius\":60}"),
            req, CancellationToken.None);

        Assert.Equal(42u, module.LastBuyGuid);
        Assert.Contains("Healing Potion", module.LastBuyBody);
        Assert.Contains("60", module.LastBuyBody);
    }

    [Fact]
    public async Task Buy_without_item_name_returns_error()
    {
        var module = new FakeModuleClient();
        var dispatcher = new ToolDispatcher(module, new PendingActions());
        IncomingRequest req = Build.Request(Build.Member("Thrall", 42));

        string result = await dispatcher.DispatchAsync(
            Build.Call("buy_item_from_vendor", "{\"bot_name\":\"Thrall\"}"),
            req, CancellationToken.None);

        Assert.Contains("item_name", result);
        Assert.DoesNotContain("buy:42", module.Calls);
    }
}
