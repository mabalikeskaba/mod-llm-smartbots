namespace BotAgent.Service.Llm;

// Bound from the "Llm" configuration section.
public sealed class LlmOptions
{
    public string Provider { get; set; } = "anthropic"; // "anthropic" | "openai"
    public string Model { get; set; } = "claude-haiku-4-5-20251001";
    public string ApiKey { get; set; } = "";
    public int MaxTokens { get; set; } = 400;

    // Language the companion replies in. A language name or code (e.g. "German",
    // "de", "Deutsch"). When empty, the bot mirrors the language the player
    // wrote in. Item names are always resolved in English regardless.
    public string Language { get; set; } = "";

    public string AnthropicBaseUrl { get; set; } = "https://api.anthropic.com";
    public string AnthropicVersion { get; set; } = "2023-06-01";
    public string OpenAiBaseUrl { get; set; } = "https://api.openai.com";
}
