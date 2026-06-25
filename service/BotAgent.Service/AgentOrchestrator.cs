using System.Text;
using BotAgent.Service.Llm;
using BotAgent.Service.Models;
using BotAgent.Service.Tools;
using Microsoft.Extensions.Options;

namespace BotAgent.Service;

// Drives one player command end to end: runs the LLM tool-calling loop, executes
// tools against the module, and posts the model's natural-language acknowledgement
// back into the party.
public sealed class AgentOrchestrator
{
    private const int MaxIterations = 5;

    private readonly ILlmProvider _llm;
    private readonly ToolDispatcher _dispatcher;
    private readonly ToolCatalog _catalog;
    private readonly IModuleClient _module;
    private readonly LlmOptions _opt;
    private readonly ILogger<AgentOrchestrator> _log;

    public AgentOrchestrator(
        ILlmProvider llm, ToolDispatcher dispatcher, ToolCatalog catalog,
        IModuleClient module, IOptions<LlmOptions> opt, ILogger<AgentOrchestrator> log)
    {
        _llm = llm;
        _dispatcher = dispatcher;
        _catalog = catalog;
        _module = module;
        _opt = opt.Value;
        _log = log;
    }

    public async Task HandleAsync(IncomingRequest req, CancellationToken ct)
    {
        string system = BuildSystemPrompt(req);
        var messages = new List<LlmMessage> { LlmMessage.FromUser(req.Message) };

        string? finalText = null;

        for (int i = 0; i < MaxIterations; i++)
        {
            LlmTurn turn = await _llm.NextAsync(system, messages, _catalog.Tools, _opt.MaxTokens, ct);

            if (turn.Stop == LlmStop.ToolUse && turn.ToolCalls.Count > 0)
            {
                messages.Add(LlmMessage.FromAssistant(turn.Text, turn.ToolCalls));
                foreach (ToolCallRequest call in turn.ToolCalls)
                {
                    string result = await _dispatcher.DispatchAsync(call, req, ct);
                    _log.LogInformation("[tool] {Tool} -> {Result}", call.Name, result);
                    messages.Add(LlmMessage.FromToolResult(call.Id, result));
                }
                continue;
            }

            finalText = turn.Text;
            break;
        }

        if (string.IsNullOrWhiteSpace(finalText))
        {
            _log.LogWarning("orchestration produced no acknowledgement text for {Player}", req.Player);
            return;
        }

        await _module.SendChatAsync(req.GroupGuid, finalText!.Trim(), ct);
    }

    // Phrases and posts the completion acknowledgement for a finished async
    // action (e.g. a vendor run reported via /action_result). No tools — one
    // natural-language sentence from the companion's point of view.
    public async Task CompleteActionAsync(ActionResult result, PendingAction ctx, CancellationToken ct)
    {
        string errand = ctx.Kind switch
        {
            "sell"   => "selling items at a vendor to free bag space",
            "give"   => "handing items to the player through a trade",
            "repair" => "repairing the companion's gear at a repair NPC",
            _        => "a shopping errand",
        };

        string system =
            $"You are {ctx.BotName}, a companion of {ctx.Player} in World of Warcraft. " +
            $"Reply with ONE short, natural, in-character sentence acknowledging the result of " +
            $"{errand}. No markdown, no quotes. " + LanguageInstruction();

        var sb = new StringBuilder();
        switch (ctx.Kind)
        {
            case "sell":
                sb.Append(result.Success ? "The items were sold. " : "Selling failed. ");
                sb.Append($"Items: {ctx.ItemName}. ");
                if (result.Price is > 0) sb.Append($"Earned: {Money.Format(result.Price.Value)}. ");
                if (!string.IsNullOrEmpty(result.Vendor)) sb.Append($"Vendor: {result.Vendor}. ");
                break;

            case "give":
                sb.Append(result.Success
                    ? "The trade window is open with the items in it; the player just needs to confirm it. "
                    : "Handing the items over failed. ");
                sb.Append($"Items: {ctx.ItemName}. ");
                break;

            case "repair":
                sb.Append(result.Success ? "The gear was repaired. " : "Repair failed. ");
                if (result.Price is > 0) sb.Append($"Cost: {Money.Format(result.Price.Value)}. ");
                if (!string.IsNullOrEmpty(result.Vendor)) sb.Append($"Repaired at: {result.Vendor}. ");
                break;

            default: // buy
                sb.Append(result.Success ? "The purchase succeeded. " : "The purchase failed. ");
                sb.Append($"Requested item: {ctx.ItemName}. ");
                if (!string.IsNullOrEmpty(result.Item)) sb.Append($"Bought: {result.Item}. ");
                if (result.Price is > 0) sb.Append($"Price: {Money.Format(result.Price.Value)}. ");
                if (!string.IsNullOrEmpty(result.Vendor)) sb.Append($"Vendor: {result.Vendor}. ");
                break;
        }
        if (!string.IsNullOrEmpty(result.Message)) sb.Append($"Detail: {result.Message}. ");
        sb.Append($"Tell {ctx.Player} the outcome.");

        var messages = new List<LlmMessage> { LlmMessage.FromUser(sb.ToString()) };
        LlmTurn turn = await _llm.NextAsync(system, messages, Array.Empty<ToolDefinition>(), _opt.MaxTokens, ct);

        string? text = turn.Text?.Trim();
        if (!string.IsNullOrWhiteSpace(text))
            await _module.SendChatAsync(ctx.GroupGuid, text!, ct);
        else
            _log.LogWarning("no completion ack text for request {Id}", result.RequestId);
    }

    private string BuildSystemPrompt(IncomingRequest req)
    {
        var sb = new StringBuilder();
        sb.Append("You operate a player's companion bots in World of Warcraft. ");
        sb.Append("Use the provided tools to read live bot data (gold, level, inventory) ");
        sb.Append("and to perform actions: buying an item from a vendor, selling items at a ");
        sb.Append("vendor to free bag space, giving carried items to the player via trade, ");
        sb.Append("and repairing the companion's gear at a repair NPC. ");
        sb.Append("To free bag space, call get_inventory first, decide which items are no longer ");
        sb.Append("needed, then sell them. To hand items to the player, use give_items_to_player. ");
        sb.Append("Only act on the companion the player refers to; pick names from the roster. ");
        sb.Append("After using tools, reply with a SHORT, natural, in-character acknowledgement ");
        sb.Append("that answers the player — one or two sentences, no markdown, no quotes. ");
        sb.Append("If a tool returns an error, acknowledge the problem plainly. ");
        sb.Append(LanguageInstruction());
        sb.Append("\n\n");

        sb.Append($"Player: {req.Player}\n");
        sb.Append("Companions in the group:\n");
        if (req.Roster.Count == 0)
        {
            sb.Append("  (none)\n");
        }
        else
        {
            foreach (RosterMember m in req.Roster)
                sb.Append($"  - {m.Name} (level {m.Level})\n");
        }
        return sb.ToString();
    }

    // Reply-language directive: a configured language pins the output language;
    // otherwise the bot mirrors whatever language the player wrote in. Item
    // names are always looked up in English by the tools regardless.
    private string LanguageInstruction()
    {
        string lang = _opt.Language?.Trim() ?? "";
        return lang.Length > 0
            ? $"Always write your reply to the player in {lang}, regardless of the language they used. "
            : "Write your reply in the same language the player used. ";
    }
}
