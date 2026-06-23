using System.Text.Json;
using BotAgent.Service;
using BotAgent.Service.Llm;
using BotAgent.Service.Models;
using BotAgent.Service.Tools;
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
builder.Services.AddHttpClient<IModuleClient, ModuleClient>((sp, http) =>
{
    ModuleClientOptions o = sp.GetRequiredService<IOptions<ModuleClientOptions>>().Value;
    http.BaseAddress = new Uri(o.BaseUrl);
    if (!string.IsNullOrEmpty(o.Token))
        http.DefaultRequestHeaders.Add("X-Agent-Token", o.Token);
});

builder.Services.AddSingleton<ToolCatalog>();
builder.Services.AddSingleton<PendingActions>();
builder.Services.AddScoped<ToolDispatcher>();
builder.Services.AddScoped<AgentOrchestrator>();

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

// Trigger webhook. Accept immediately and run the (possibly multi-second) LLM
// tool-calling loop in the background so the module's sender thread is not held.
app.MapPost("/incoming", (IncomingRequest req, IServiceScopeFactory scopeFactory, ILogger<Program> log) =>
{
    log.LogInformation(
        "[/incoming] {Player} (group {Group}) said: {Message} - roster of {Count}",
        req.Player, req.GroupGuid, req.Message, req.Roster.Count);

    _ = Task.Run(async () =>
    {
        using IServiceScope scope = scopeFactory.CreateScope();
        AgentOrchestrator orchestrator = scope.ServiceProvider.GetRequiredService<AgentOrchestrator>();
        try
        {
            await orchestrator.HandleAsync(req, CancellationToken.None);
        }
        catch (Exception ex)
        {
            log.LogError(ex, "orchestration failed for {Player}", req.Player);
        }
    });

    return Results.Accepted();
});

// Async action result from the module. Correlate to the original command and
// post the completion acknowledgement; run the LLM phrasing in the background.
app.MapPost("/action_result", (ActionResult res, PendingActions pending, IServiceScopeFactory scopeFactory, ILogger<Program> log) =>
{
    if (!pending.TryTake(res.RequestId, out PendingAction ctx))
    {
        log.LogWarning("[/action_result] unknown request_id {Id}", res.RequestId);
        return Results.Ok();
    }

    _ = Task.Run(async () =>
    {
        using IServiceScope scope = scopeFactory.CreateScope();
        AgentOrchestrator orchestrator = scope.ServiceProvider.GetRequiredService<AgentOrchestrator>();
        try
        {
            await orchestrator.CompleteActionAsync(res, ctx, CancellationToken.None);
        }
        catch (Exception ex)
        {
            log.LogError(ex, "action-result ack failed for request {Id}", res.RequestId);
        }
    });

    return Results.Ok();
});

app.Run();
