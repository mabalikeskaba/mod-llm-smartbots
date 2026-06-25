using System.Text.Json;
using BotAgent.Service.Llm;
using BotAgent.Service.Tools;
using Xunit;

namespace BotAgent.Tests;

public class ToolCatalogTests
{
    [Fact]
    public void Exposes_the_expected_tools()
    {
        var catalog = new ToolCatalog();
        var names = catalog.Tools.Select(t => t.Name).ToHashSet();

        Assert.Equal(
            new HashSet<string>
            {
                "get_gold", "get_level", "get_inventory",
                "buy_item_from_vendor", "sell_items_to_vendor", "give_items_to_player",
                "repair_equipment", "move_companion",
            },
            names);
    }

    [Fact]
    public void Every_schema_is_an_object_requiring_bot_name()
    {
        foreach (ToolDefinition tool in new ToolCatalog().Tools)
        {
            Assert.Equal(JsonValueKind.Object, tool.InputSchema.ValueKind);

            JsonElement required = tool.InputSchema.GetProperty("required");
            var names = required.EnumerateArray().Select(e => e.GetString()).ToList();
            Assert.Contains("bot_name", names);
        }
    }

    [Fact]
    public void Buy_tool_requires_item_name()
    {
        ToolDefinition buy = new ToolCatalog().Tools.Single(t => t.Name == "buy_item_from_vendor");
        var required = buy.InputSchema.GetProperty("required").EnumerateArray()
            .Select(e => e.GetString()).ToList();

        Assert.Contains("item_name", required);
    }
}
