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
                RegisterPendingIfAccepted(buyResult, req, bot.Name, item!);
                return buyResult;
            }

            default:
                return Err($"unknown tool '{call.Name}'");
        }
    }

    // If the module accepted the async buy (accepted=true + request_id), remember
    // the context so the /action_result callback can produce the completion ack.
    private void RegisterPendingIfAccepted(string buyResult, IncomingRequest req, string botName, string itemName)
    {
        try
        {
            using var doc = JsonDocument.Parse(buyResult);
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
                _pending.Register(new PendingAction(id, req.GroupGuid, req.Player, botName, itemName));
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

    private static string? GetString(JsonElement args, string prop) =>
        args.ValueKind == JsonValueKind.Object &&
        args.TryGetProperty(prop, out JsonElement v) &&
        v.ValueKind == JsonValueKind.String
            ? v.GetString()
            : null;

    private static string Err(string reason) =>
        new JsonObject { ["error"] = reason }.ToJsonString();
}
