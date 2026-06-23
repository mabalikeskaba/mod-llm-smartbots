using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace BotAgent.Service.Llm;

// OpenAI Chat Completions API with native tool calls.
public sealed class OpenAiProvider : ILlmProvider
{
    private readonly IHttpClientFactory _factory;
    private readonly LlmOptions _opt;

    public OpenAiProvider(IHttpClientFactory factory, LlmOptions opt)
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
            ["messages"] = BuildMessages(system, messages),
        };

        if (tools.Count > 0)
        {
            var arr = new JsonArray();
            foreach (ToolDefinition t in tools)
                arr.Add(new JsonObject
                {
                    ["type"] = "function",
                    ["function"] = new JsonObject
                    {
                        ["name"] = t.Name,
                        ["description"] = t.Description,
                        ["parameters"] = JsonNode.Parse(t.InputSchema.GetRawText()),
                    },
                });
            root["tools"] = arr;
            root["tool_choice"] = "auto";
        }

        var client = _factory.CreateClient();
        using var req = new HttpRequestMessage(HttpMethod.Post,
            new Uri(new Uri(_opt.OpenAiBaseUrl), "/v1/chat/completions"));
        req.Headers.Authorization = new AuthenticationHeaderValue("Bearer", _opt.ApiKey);
        req.Content = new StringContent(root.ToJsonString(), Encoding.UTF8, "application/json");

        using var resp = await client.SendAsync(req, ct);
        string body = await resp.Content.ReadAsStringAsync(ct);
        if (!resp.IsSuccessStatusCode)
            throw new HttpRequestException($"OpenAI API {(int)resp.StatusCode}: {body}");

        return ParseResponse(body);
    }

    private static JsonArray BuildMessages(string system, IReadOnlyList<LlmMessage> messages)
    {
        var arr = new JsonArray();
        if (!string.IsNullOrEmpty(system))
            arr.Add(new JsonObject { ["role"] = "system", ["content"] = system });

        foreach (LlmMessage m in messages)
        {
            switch (m.Role)
            {
                case LlmRole.User:
                    arr.Add(new JsonObject { ["role"] = "user", ["content"] = m.Text ?? "" });
                    break;

                case LlmRole.Assistant:
                {
                    var obj = new JsonObject
                    {
                        ["role"] = "assistant",
                        ["content"] = string.IsNullOrEmpty(m.Text) ? null : m.Text,
                    };
                    if (m.ToolCalls != null && m.ToolCalls.Count > 0)
                    {
                        var calls = new JsonArray();
                        foreach (ToolCallRequest c in m.ToolCalls)
                            calls.Add(new JsonObject
                            {
                                ["id"] = c.Id,
                                ["type"] = "function",
                                ["function"] = new JsonObject
                                {
                                    ["name"] = c.Name,
                                    // OpenAI requires arguments as a JSON-encoded string.
                                    ["arguments"] = c.Arguments.GetRawText(),
                                },
                            });
                        obj["tool_calls"] = calls;
                    }
                    arr.Add(obj);
                    break;
                }

                default: // Tool
                    arr.Add(new JsonObject
                    {
                        ["role"] = "tool",
                        ["tool_call_id"] = m.ToolCallId,
                        ["content"] = m.ToolResultContent ?? "",
                    });
                    break;
            }
        }
        return arr;
    }

    private static LlmTurn ParseResponse(string body)
    {
        using var doc = JsonDocument.Parse(body);
        JsonElement choice = doc.RootElement.GetProperty("choices")[0];
        JsonElement message = choice.GetProperty("message");

        string? text = message.TryGetProperty("content", out JsonElement c) && c.ValueKind == JsonValueKind.String
            ? c.GetString()
            : null;

        var calls = new List<ToolCallRequest>();
        if (message.TryGetProperty("tool_calls", out JsonElement tcs) && tcs.ValueKind == JsonValueKind.Array)
        {
            foreach (JsonElement tc in tcs.EnumerateArray())
            {
                string id = tc.GetProperty("id").GetString() ?? "";
                JsonElement fn = tc.GetProperty("function");
                string name = fn.GetProperty("name").GetString() ?? "";
                string? argsJson = fn.TryGetProperty("arguments", out JsonElement a) ? a.GetString() : null;
                calls.Add(new ToolCallRequest(id, name, JsonHelpers.ParseOrEmpty(argsJson)));
            }
        }

        string? finish = choice.TryGetProperty("finish_reason", out JsonElement fr) ? fr.GetString() : null;
        LlmStop stop = finish == "tool_calls" ? LlmStop.ToolUse : LlmStop.EndTurn;
        return new LlmTurn(text, calls, stop);
    }
}
