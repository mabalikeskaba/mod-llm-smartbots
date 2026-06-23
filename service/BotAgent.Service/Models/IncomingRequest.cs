namespace BotAgent.Service.Models;

// One companion in the player's group, as forwarded by the C++ module.
public sealed record RosterMember(
    uint Guid,
    string Name,
    int Class,
    int Level,
    uint Map);

// Payload the module POSTs to /incoming when a player issues a prefixed command
// in party/raid chat. Field names are snake_case on the wire (configured via the
// JSON naming policy in Program.cs). group_guid is the raw GUID as a string.
public sealed record IncomingRequest(
    string Player,
    uint PlayerGuid,
    string GroupGuid,
    string Message,
    List<RosterMember> Roster);
