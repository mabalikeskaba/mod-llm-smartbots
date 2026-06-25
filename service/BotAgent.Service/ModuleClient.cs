using System.Net.Http;
using System.Text;
using System.Text.Json.Nodes;

namespace BotAgent.Service;

// Bound from the "Module" configuration section.
public sealed class ModuleClientOptions
{
    public string BaseUrl { get; set; } = "http://127.0.0.1:8810";
    public string Token { get; set; } = "";
}

// Talks to the C++ module. Abstracted so the orchestrator/dispatcher can be
// unit-tested without a live server.
public interface IModuleClient
{
    Task<string> GetGoldAsync(uint guid, CancellationToken ct);
    Task<string> GetLevelAsync(uint guid, CancellationToken ct);
    Task<string> GetInventoryAsync(uint guid, CancellationToken ct);
    Task<string> BuyAsync(uint guid, string jsonBody, CancellationToken ct);
    Task<string> SellAsync(uint guid, string jsonBody, CancellationToken ct);
    Task<string> TradeAsync(uint guid, string jsonBody, CancellationToken ct);
    Task<string> SendChatAsync(string groupGuid, string text, CancellationToken ct);
}

// Typed HttpClient talking to the C++ module. The base address and the
// X-Agent-Token header are configured on the injected HttpClient (see
// Program.cs). Read/buy bodies are returned verbatim — error JSON from the
// module is meaningful to the LLM and is passed through as a tool result.
public sealed class ModuleClient : IModuleClient
{
    private readonly HttpClient _http;

    public ModuleClient(HttpClient http) => _http = http;

    public Task<string> GetGoldAsync(uint guid, CancellationToken ct) =>
        GetAsync($"/bot/{guid}/gold", ct);

    public Task<string> GetLevelAsync(uint guid, CancellationToken ct) =>
        GetAsync($"/bot/{guid}/level", ct);

    public Task<string> GetInventoryAsync(uint guid, CancellationToken ct) =>
        GetAsync($"/bot/{guid}/inventory", ct);

    public Task<string> BuyAsync(uint guid, string jsonBody, CancellationToken ct) =>
        PostAsync($"/bot/{guid}/buy", jsonBody, ct);

    public Task<string> SellAsync(uint guid, string jsonBody, CancellationToken ct) =>
        PostAsync($"/bot/{guid}/sell", jsonBody, ct);

    public Task<string> TradeAsync(uint guid, string jsonBody, CancellationToken ct) =>
        PostAsync($"/bot/{guid}/trade", jsonBody, ct);

    public Task<string> SendChatAsync(string groupGuid, string text, CancellationToken ct)
    {
        string body = new JsonObject { ["group_guid"] = groupGuid, ["text"] = text }.ToJsonString();
        return PostAsync("/chat", body, ct);
    }

    private async Task<string> GetAsync(string path, CancellationToken ct)
    {
        using HttpResponseMessage resp = await _http.GetAsync(path, ct);
        return await resp.Content.ReadAsStringAsync(ct);
    }

    private async Task<string> PostAsync(string path, string json, CancellationToken ct)
    {
        using var content = new StringContent(json, Encoding.UTF8, "application/json");
        using HttpResponseMessage resp = await _http.PostAsync(path, content, ct);
        return await resp.Content.ReadAsStringAsync(ct);
    }
}
