using System.Text.Json;

namespace BotAgent.Service.Llm;

public enum LlmRole { User, Assistant, Tool }
public enum LlmStop { EndTurn, ToolUse }

// A tool the model may call. InputSchema is a JSON Schema object.
public sealed record ToolDefinition(string Name, string Description, JsonElement InputSchema);

// A tool call the model emitted. Arguments is the (already-parsed) JSON object.
public sealed record ToolCallRequest(string Id, string Name, JsonElement Arguments);

// One message in the provider-agnostic conversation. The providers translate
// these into their own wire formats.
public sealed class LlmMessage
{
    public LlmRole Role { get; init; }
    public string? Text { get; init; }                          // user / assistant text
    public IReadOnlyList<ToolCallRequest>? ToolCalls { get; init; } // assistant tool calls
    public string? ToolCallId { get; init; }                    // tool result -> which call
    public string? ToolResultContent { get; init; }             // tool result body

    public static LlmMessage FromUser(string text) =>
        new() { Role = LlmRole.User, Text = text };

    public static LlmMessage FromAssistant(string? text, IReadOnlyList<ToolCallRequest>? calls) =>
        new() { Role = LlmRole.Assistant, Text = text, ToolCalls = calls };

    public static LlmMessage FromToolResult(string toolCallId, string content) =>
        new() { Role = LlmRole.Tool, ToolCallId = toolCallId, ToolResultContent = content };
}

// The result of one model turn.
public sealed record LlmTurn(string? Text, IReadOnlyList<ToolCallRequest> ToolCalls, LlmStop Stop);

// Provider-agnostic chat+tools entry point. One call = one model turn.
public interface ILlmProvider
{
    Task<LlmTurn> NextAsync(
        string system,
        IReadOnlyList<LlmMessage> messages,
        IReadOnlyList<ToolDefinition> tools,
        int maxTokens,
        CancellationToken ct);
}
