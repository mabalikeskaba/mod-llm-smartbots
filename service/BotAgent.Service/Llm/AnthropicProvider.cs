using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace BotAgent.Service.Llm;

// Anthropic Messages API with native tool use.
public sealed class AnthropicProvider : ILlmProvider
{
    private readonly IHttpClientFactory _factory;
    private readonly LlmOptions _opt;

    public AnthropicProvider(IHttpClientFactory factory, LlmOptions opt)
    {
        _factory = factory;
        _opt = opt;
    }

    public async Task<LlmTurn> NextAsync(
        string system, IReadOnlyList<LlmMessage> messages,
        IReadOnlyList<ToolDefinition> tools, int maxTokens, CancellationToken ct)
    {
        var root = new JsonObject
        {
            ["model"] = _opt.Model,
            ["max_tokens"] = maxTokens,
        };
        if (!string.IsNullOrEmpty(system))
            root["system"] = system;

        if (tools.Count > 0)
        {
            var arr = new JsonArray();
            foreach (var t in tools)
                arr.Add(new JsonObject
                {
                    ["name"] = t.Name,
                    ["description"] = t.Description,
                    ["input_schema"] = JsonNode.Parse(t.InputSchema.GetRawText()),
                });
            root["tools"] = arr;
        }

        root["messages"] = BuildMessages(messages);

        var client = _factory.CreateClient();
        using var req = new HttpRequestMessage(HttpMethod.Post,
            new Uri(new Uri(_opt.AnthropicBaseUrl), "/v1/messages"));
        req.Headers.Add("x-api-key", _opt.ApiKey);
        req.Headers.Add("anthropic-version", _opt.AnthropicVersion);
        req.Content = new StringContent(root.ToJsonString(), Encoding.UTF8, "application/json");

        using var resp = await client.SendAsync(req, ct);
        string body = await resp.Content.ReadAsStringAsync(ct);
        if (!resp.IsSuccessStatusCode)
            throw new HttpRequestException($"Anthropic API {(int)resp.StatusCode}: {body}");

        return ParseResponse(body);
    }

    private static JsonArray BuildMessages(IReadOnlyList<LlmMessage> messages)
    {
        var arr = new JsonArray();
        int i = 0;
        while (i < messages.Count)
        {
            LlmMessage m = messages[i];
            switch (m.Role)
            {
                case LlmRole.User:
                    arr.Add(new JsonObject
                    {
                        ["role"] = "user",
                        ["content"] = new JsonArray
                        {
                            new JsonObject { ["type"] = "text", ["text"] = m.Text ?? "" },
                        },
                    });
                    i++;
                    break;

                case LlmRole.Assistant:
                {
                    var content = new JsonArray();
                    if (!string.IsNullOrEmpty(m.Text))
                        content.Add(new JsonObject { ["type"] = "text", ["text"] = m.Text });
                    if (m.ToolCalls != null)
                        foreach (ToolCallRequest c in m.ToolCalls)
                            content.Add(new JsonObject
                            {
                                ["type"] = "tool_use",
                                ["id"] = c.Id,
                                ["name"] = c.Name,
                                ["input"] = JsonNode.Parse(c.Arguments.GetRawText()),
                            });
                    arr.Add(new JsonObject { ["role"] = "assistant", ["content"] = content });
                    i++;
                    break;
                }

                default: // Tool — group consecutive tool results into one user message
                {
                    var content = new JsonArray();
                    while (i < messages.Count && messages[i].Role == LlmRole.Tool)
                    {
                        LlmMessage tm = messages[i];
                        content.Add(new JsonObject
                        {
                            ["type"] = "tool_result",
                            ["tool_use_id"] = tm.ToolCallId,
                            ["content"] = tm.ToolResultContent ?? "",
                        });
                        i++;
                    }
                    arr.Add(new JsonObject { ["role"] = "user", ["content"] = content });
                    break;
                }
            }
        }
        return arr;
    }

    private static LlmTurn ParseResponse(string body)
    {
        using var doc = JsonDocument.Parse(body);
        JsonElement root = doc.RootElement;

        string? text = null;
        var calls = new List<ToolCallRequest>();

        if (root.TryGetProperty("content", out JsonElement content) &&
            content.ValueKind == JsonValueKind.Array)
        {
            foreach (JsonElement block in content.EnumerateArray())
            {
                string? type = block.GetProperty("type").GetString();
                if (type == "text")
                    text = (text ?? "") + block.GetProperty("text").GetString();
                else if (type == "tool_use")
                {
                    string id = block.GetProperty("id").GetString() ?? "";
                    string name = block.GetProperty("name").GetString() ?? "";
                    JsonElement input = block.TryGetProperty("input", out JsonElement inp)
                        ? inp.Clone()
                        : JsonHelpers.EmptyObject();
                    calls.Add(new ToolCallRequest(id, name, input));
                }
            }
        }

        string? stopReason = root.TryGetProperty("stop_reason", out JsonElement sr) ? sr.GetString() : null;
        LlmStop stop = stopReason == "tool_use" ? LlmStop.ToolUse : LlmStop.EndTurn;
        return new LlmTurn(text, calls, stop);
    }
}
