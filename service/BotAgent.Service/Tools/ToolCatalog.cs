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
            Define("sell_items_to_vendor",
                "Send a companion to the nearest vendor on their map and sell the named items " +
                "to free up bag space. Use get_inventory first to decide which items are no " +
                "longer needed (e.g. junk, vendor trash, low-value or surplus items), then pass " +
                "their English item names. Returns immediately; completes asynchronously with a " +
                "follow-up acknowledgement.",
                SellSchema),
            Define("give_items_to_player",
                "Send a companion back to the player and open a trade to hand over the named " +
                "items the companion is carrying. The companion walks to the player, puts the " +
                "items in the trade window and accepts its side; the player confirms on their " +
                "client to finish. Returns immediately; completes asynchronously.",
                GiveSchema),
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
        "item_name": { "type": "string", "description": "The item's official English World of Warcraft (WotLK) name, e.g. 'Mild Spices'. The item lookup is English-only — translate or convert the player's wording into the correct English in-game item name before calling this tool, even if the player asked in another language." },
        "radius": { "type": "integer", "description": "Optional max search radius in yards on the bot's map. Omit for no cap." }
      },
      "required": ["bot_name", "item_name"]
    }
    """;

    private const string SellSchema = """
    {
      "type": "object",
      "properties": {
        "bot_name": { "type": "string", "description": "Name of the companion (from the roster)." },
        "item_names": {
          "type": "array",
          "items": { "type": "string" },
          "description": "Official English WotLK item names to sell, e.g. ['Broken Fang', 'Tattered Cloth']. The item lookup is English-only — translate the player's wording to the correct English in-game names."
        },
        "radius": { "type": "integer", "description": "Optional max search radius in yards on the bot's map. Omit for no cap." }
      },
      "required": ["bot_name", "item_names"]
    }
    """;

    private const string GiveSchema = """
    {
      "type": "object",
      "properties": {
        "bot_name": { "type": "string", "description": "Name of the companion (from the roster)." },
        "item_names": {
          "type": "array",
          "items": { "type": "string" },
          "description": "Official English WotLK item names to hand over to the player, e.g. ['Mild Spices']. The companion must already be carrying them. The item lookup is English-only."
        }
      },
      "required": ["bot_name", "item_names"]
    }
    """;

    private static ToolDefinition Define(string name, string description, string schemaJson)
    {
        using var doc = JsonDocument.Parse(schemaJson);
        return new ToolDefinition(name, description, doc.RootElement.Clone());
    }
}
