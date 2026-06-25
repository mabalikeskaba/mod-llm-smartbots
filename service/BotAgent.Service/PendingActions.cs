using System.Collections.Concurrent;

namespace BotAgent.Service;

// Context for an async action awaiting its /action_result callback, keyed by the
// request_id the module returned.
public sealed record PendingAction(
    string RequestId,
    string GroupGuid,
    string Player,
    string BotName,
    string ItemName,
    string Kind = "buy"); // "buy" | "sell" | "give" — picks the completion phrasing

// In-memory correlation store for in-flight async actions (e.g. a vendor run).
public sealed class PendingActions
{
    private readonly ConcurrentDictionary<string, PendingAction> _map = new();

    public void Register(PendingAction action) => _map[action.RequestId] = action;

    public bool TryTake(string requestId, out PendingAction action) =>
        _map.TryRemove(requestId, out action!);

    public int Count => _map.Count;
}
