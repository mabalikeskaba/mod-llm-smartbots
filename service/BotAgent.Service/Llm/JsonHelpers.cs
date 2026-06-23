using System.Text.Json;

namespace BotAgent.Service.Llm;

internal static class JsonHelpers
{
    private static readonly JsonElement _empty = JsonDocument.Parse("{}").RootElement.Clone();

    // A detached, reusable empty JSON object element.
    public static JsonElement EmptyObject() => _empty;

    // Parses a JSON string into a detached element, falling back to {} on error.
    public static JsonElement ParseOrEmpty(string? json)
    {
        if (string.IsNullOrWhiteSpace(json))
            return _empty;
        try
        {
            using var doc = JsonDocument.Parse(json);
            return doc.RootElement.Clone();
        }
        catch (JsonException)
        {
            return _empty;
        }
    }
}
