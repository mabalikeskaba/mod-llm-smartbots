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

    public ToolDispatcher(IModuleClient module) => _module = module;

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

                return await _module.BuyAsync(bot.Guid, body.ToJsonString(), ct);
            }

            default:
                return Err($"unknown tool '{call.Name}'");
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
