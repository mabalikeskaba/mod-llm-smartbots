using System.Text.Json;
using System.Text.Json.Nodes;
using BotAgent.Service.Llm;
using BotAgent.Service.Models;

namespace BotAgent.Service.Tools;

// Executes a tool call against the C++ module. Resolves the target companion's
// name (from the request roster) to a GUID, then calls the matching endpoint.
// Always returns a JSON string to feed back to the model as the tool result.
public sealed class ToolDispatcher
{
    private readonly IModuleClient _module;
    private readonly PendingActions _pending;

    public ToolDispatcher(IModuleClient module, PendingActions pending)
    {
        _module = module;
        _pending = pending;
    }

    public async Task<string> DispatchAsync(ToolCallRequest call, IncomingRequest req, CancellationToken ct)
    {
        string? botName = GetString(call.Arguments, "bot_name");
        if (string.IsNullOrWhiteSpace(botName))
            return Err("missing 'bot_name'");

        RosterMember? bot = ResolveBot(req, botName!);
        if (bot is null)
            return Err($"no companion named '{botName}' in the group");

        switch (call.Name)
        {
            case "get_gold":
                return await _module.GetGoldAsync(bot.Guid, ct);

            case "get_level":
                return await _module.GetLevelAsync(bot.Guid, ct);

            case "get_inventory":
                return await _module.GetInventoryAsync(bot.Guid, ct);

            case "buy_item_from_vendor":
            {
                string? item = GetString(call.Arguments, "item_name");
                if (string.IsNullOrWhiteSpace(item))
                    return Err("missing 'item_name'");

                var body = new JsonObject { ["item_name"] = item };
                if (call.Arguments.TryGetProperty("radius", out JsonElement r) &&
                    r.ValueKind == JsonValueKind.Number && r.TryGetInt32(out int radius))
                    body["radius"] = radius;

                string buyResult = await _module.BuyAsync(bot.Guid, body.ToJsonString(), ct);
                RegisterPendingIfAccepted(buyResult, req, bot.Name, item!, "buy");
                return buyResult;
            }

            case "sell_items_to_vendor":
            {
                List<string> names = GetStringArray(call.Arguments, "item_names");
                if (names.Count == 0)
                    return Err("missing 'item_names'");

                var body = new JsonObject { ["items"] = string.Join('|', names) };
                if (call.Arguments.TryGetProperty("radius", out JsonElement r) &&
                    r.ValueKind == JsonValueKind.Number && r.TryGetInt32(out int radius))
                    body["radius"] = radius;

                string sellResult = await _module.SellAsync(bot.Guid, body.ToJsonString(), ct);
                RegisterPendingIfAccepted(sellResult, req, bot.Name, string.Join(", ", names), "sell");
                return sellResult;
            }

            case "give_items_to_player":
            {
                List<string> names = GetStringArray(call.Arguments, "item_names");
                if (names.Count == 0)
                    return Err("missing 'item_names'");

                var body = new JsonObject
                {
                    ["player_guid"] = req.PlayerGuid,
                    ["items"] = string.Join('|', names),
                };

                string giveResult = await _module.TradeAsync(bot.Guid, body.ToJsonString(), ct);
                RegisterPendingIfAccepted(giveResult, req, bot.Name, string.Join(", ", names), "give");
                return giveResult;
            }

            case "repair_equipment":
            {
                string repairResult = await _module.RepairAsync(bot.Guid, "{}", ct);
                RegisterPendingIfAccepted(repairResult, req, bot.Name, "", "repair");
                return repairResult;
            }

            case "move_companion":
            {
                string? action = GetString(call.Arguments, "action");
                if (action is not ("come" or "follow" or "stay"))
                    return Err("'action' must be one of: come, follow, stay");

                var body = new JsonObject { ["command"] = action, ["player_guid"] = req.PlayerGuid };
                // Takes effect immediately; no async callback to correlate.
                return await _module.MoveAsync(bot.Guid, body.ToJsonString(), ct);
            }

            default:
                return Err($"unknown tool '{call.Name}'");
        }
    }

    // If the module accepted the async action (accepted=true + request_id),
    // remember the context so the /action_result callback can phrase the
    // completion acknowledgement. `kind` selects the phrasing (buy/sell/give).
    private void RegisterPendingIfAccepted(string result, IncomingRequest req, string botName, string itemName, string kind)
    {
        try
        {
            using var doc = JsonDocument.Parse(result);
            JsonElement root = doc.RootElement;
            if (root.ValueKind != JsonValueKind.Object)
                return;

            bool accepted = root.TryGetProperty("accepted", out JsonElement a) &&
                            a.ValueKind == JsonValueKind.True;
            if (!accepted)
                return;

            if (root.TryGetProperty("request_id", out JsonElement idEl) &&
                idEl.ValueKind == JsonValueKind.String &&
                idEl.GetString() is { Length: > 0 } id)
            {
                _pending.Register(new PendingAction(id, req.GroupGuid, req.Player, botName, itemName, kind));
            }
        }
        catch (JsonException)
        {
            // Non-JSON / error body — nothing to correlate.
        }
    }

    private static RosterMember? ResolveBot(IncomingRequest req, string name)
    {
        foreach (RosterMember m in req.Roster)
            if (string.Equals(m.Name, name, StringComparison.OrdinalIgnoreCase))
                return m;
        return null;
    }

    // Reads a string array argument. Also tolerates a single string (some models
    // pass one item as a bare string instead of a one-element array).
    private static List<string> GetStringArray(JsonElement args, string prop)
    {
        var list = new List<string>();
        if (args.ValueKind != JsonValueKind.Object || !args.TryGetProperty(prop, out JsonElement v))
            return list;

        if (v.ValueKind == JsonValueKind.Array)
        {
            foreach (JsonElement e in v.EnumerateArray())
                if (e.ValueKind == JsonValueKind.String && e.GetString() is { Length: > 0 } s)
                    list.Add(s);
        }
        else if (v.ValueKind == JsonValueKind.String && v.GetString() is { Length: > 0 } single)
        {
            list.Add(single);
        }
        return list;
    }

    private static string? GetString(JsonElement args, string prop) =>
        args.ValueKind == JsonValueKind.Object &&
        args.TryGetProperty(prop, out JsonElement v) &&
        v.ValueKind == JsonValueKind.String
            ? v.GetString()
            : null;

    private static string Err(string reason) =>
        new JsonObject { ["error"] = reason }.ToJsonString();
}
