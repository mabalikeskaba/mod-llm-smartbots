using System.Text.Json;
using BotAgent.Service;
using BotAgent.Service.Llm;
using BotAgent.Service.Models;

namespace BotAgent.Tests;

internal static class Json
{
    public static JsonElement Parse(string s) => JsonDocument.Parse(s).RootElement.Clone();
}

// Records every call and returns canned bodies.
internal sealed class FakeModuleClient : IModuleClient
{
    public List<string> Calls { get; } = new();
    public uint? LastBuyGuid;
    public string? LastBuyBody;
    public string? LastChatGroup;
    public string? LastChatText;

    public string GoldResult = "{\"copper\":0}";
    public string LevelResult = "{\"level\":1}";
    public string InventoryResult = "{\"items\":[]}";
    public string BuyResult = "{\"accepted\":true,\"request_id\":\"r1\"}";

    public Task<string> GetGoldAsync(uint guid, CancellationToken ct)
    {
        Calls.Add($"gold:{guid}");
        return Task.FromResult(GoldResult);
    }

    public Task<string> GetLevelAsync(uint guid, CancellationToken ct)
    {
        Calls.Add($"level:{guid}");
        return Task.FromResult(LevelResult);
    }

    public Task<string> GetInventoryAsync(uint guid, CancellationToken ct)
    {
        Calls.Add($"inventory:{guid}");
        return Task.FromResult(InventoryResult);
    }

    public Task<string> BuyAsync(uint guid, string jsonBody, CancellationToken ct)
    {
        LastBuyGuid = guid;
        LastBuyBody = jsonBody;
        Calls.Add($"buy:{guid}");
        return Task.FromResult(BuyResult);
    }

    public Task<string> SendChatAsync(string groupGuid, string text, CancellationToken ct)
    {
        LastChatGroup = groupGuid;
        LastChatText = text;
        Calls.Add("chat");
        return Task.FromResult(string.Empty);
    }
}

// Returns a predetermined sequence of turns.
internal sealed class FakeLlmProvider : ILlmProvider
{
    private readonly Queue<LlmTurn> _turns;
    public int CallCount { get; private set; }

    public FakeLlmProvider(params LlmTurn[] turns) => _turns = new Queue<LlmTurn>(turns);

    public Task<LlmTurn> NextAsync(
        string system, IReadOnlyList<LlmMessage> messages,
        IReadOnlyList<ToolDefinition> tools, int maxTokens, CancellationToken ct)
    {
        CallCount++;
        return Task.FromResult(_turns.Dequeue());
    }
}

internal static class Build
{
    public static IncomingRequest Request(params RosterMember[] roster) =>
        new("Hero", 1, "12345", "do the thing", roster.ToList());

    public static RosterMember Member(string name, uint guid, int level = 10) =>
        new(guid, name, 1, level, 0);

    public static ToolCallRequest Call(string name, string argsJson, string id = "c1") =>
        new(id, name, Json.Parse(argsJson));
}
