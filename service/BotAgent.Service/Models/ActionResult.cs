namespace BotAgent.Service.Models;

// Async action outcome the C++ module POSTs to /action_result when a buy
// (or future long-running action) finishes. Price is in copper.
public sealed record ActionResult(
    string RequestId,
    bool Success,
    string? Item,
    long? Price,
    string? Vendor,
    string? Message);
