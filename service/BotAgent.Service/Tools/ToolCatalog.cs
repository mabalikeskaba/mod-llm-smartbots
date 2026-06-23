using System.Text.Json;
using BotAgent.Service.Llm;

namespace BotAgent.Service.Tools;

// The fixed set of tools exposed to the model. Each tool identifies the target
// companion by name (the model picks from the roster); the dispatcher resolves
// the name to a GUID.
public sealed class ToolCatalog
{
    public IReadOnlyList<ToolDefinition> Tools { get; }

    public ToolCatalog()
    {
        Tools = new List<ToolDefinition>
        {
            Define("get_gold",
                "Get the gold/money a companion is currently carrying.",
                BotOnlySchema),
            Define("get_level",
                "Get a companion's current character level.",
                BotOnlySchema),
            Define("get_inventory",
                "List the items a companion is currently carrying.",
                BotOnlySchema),
            Define("buy_item_from_vendor",
                "Send a companion to the nearest vendor on their map that sells the named " +
                "item and buy one. Returns immediately; the purchase completes asynchronously " +
                "and a follow-up acknowledgement is sent when it finishes.",
                BuySchema),
        };
    }

    private const string BotOnlySchema = """
    {
      "type": "object",
      "properties": {
        "bot_name": { "type": "string", "description": "Name of the companion (from the roster)." }
      },
      "required": ["bot_name"]
    }
    """;

    private const string BuySchema = """
    {
      "type": "object",
      "properties": {
        "bot_name": { "type": "string", "description": "Name of the companion (from the roster)." },
        "item_name": { "type": "string", "description": "Name of the item to buy (as the player referred to it)." },
        "radius": { "type": "integer", "description": "Optional max search radius in yards on the bot's map. Omit for no cap." }
      },
      "required": ["bot_name", "item_name"]
    }
    """;

    private static ToolDefinition Define(string name, string description, string schemaJson)
    {
        using var doc = JsonDocument.Parse(schemaJson);
        return new ToolDefinition(name, description, doc.RootElement.Clone());
    }
}
