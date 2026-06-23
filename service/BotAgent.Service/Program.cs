using System.Text.Json;
using BotAgent.Service;
using BotAgent.Service.Llm;
using BotAgent.Service.Models;
using Microsoft.Extensions.Options;

var builder = WebApplication.CreateBuilder(args);

// Wire JSON to the snake_case shape the C++ module uses.
builder.Services.ConfigureHttpJsonOptions(o =>
{
    o.SerializerOptions.PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower;
    o.SerializerOptions.PropertyNameCaseInsensitive = true;
});

builder.Services.Configure<LlmOptions>(builder.Configuration.GetSection("Llm"));
builder.Services.Configure<ModuleClientOptions>(builder.Configuration.GetSection("Module"));

// Typed HttpClient to the C++ module (base address + shared token).
builder.Services.AddHttpClient<ModuleClient>((sp, http) =>
{
    ModuleClientOptions o = sp.GetRequiredService<IOptions<ModuleClientOptions>>().Value;
    http.BaseAddress = new Uri(o.BaseUrl);
    if (!string.IsNullOrEmpty(o.Token))
        http.DefaultRequestHeaders.Add("X-Agent-Token", o.Token);
});

// Default factory for the LLM providers' outbound calls.
builder.Services.AddHttpClient();

// Provider chosen by config.
builder.Services.AddSingleton<ILlmProvider>(sp =>
{
    LlmOptions o = sp.GetRequiredService<IOptions<LlmOptions>>().Value;
    IHttpClientFactory factory = sp.GetRequiredService<IHttpClientFactory>();
    return o.Provider.Trim().ToLowerInvariant() switch
    {
        "openai" => new OpenAiProvider(factory, o),
        _ => new AnthropicProvider(factory, o),
    };
});

WebApplication app = builder.Build();

app.MapGet("/healthz", () => Results.Ok(new { status = "ok" }));

// Trigger webhook. The orchestrator is wired in a later unit; for now we accept
// and log so the module<->service round-trip can be exercised end to end.
app.MapPost("/incoming", (IncomingRequest req, ILogger<Program> log) =>
{
    log.LogInformation(
        "[/incoming] {Player} (group {Group}) said: {Message} - roster of {Count}",
        req.Player, req.GroupGuid, req.Message, req.Roster.Count);
    return Results.Accepted();
});

app.Run();
