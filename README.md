# mod-bot-agent

A standalone, focused companion to [AzerothCore](https://www.azerothcore.org/) +
[mod-playerbots](https://github.com/mod-playerbots/mod-playerbots) that lets an
external **LLM service drive bot reads and actions** and have the bot acknowledge
in party chat — in natural language.

This is **not** a roleplay/ambient chatter module. It does one thing: expose live
bot data and a small set of actions to an out-of-process agent, and post the
agent's acknowledgement back into the player's party.

## Two components

```
+------------------+        HTTP (webhook)         +-------------------+
|  C++ AC module   |  --POST /incoming-----------> |   C# LLM service  |
|  (in worldserver)| <--GET/POST reads/actions/--- |   (.NET 8)        |
|                  | <--POST /chat (LLM ack)------ |                   |
+------------------+        HTTP (callback)        +-------------------+
        |  POST /action_result (async action result) ^
        +--------------------------------------------+
```

- **`module/`** — AzerothCore C++ module. Hosts a small HTTP server (bound to
  `127.0.0.1`, token-protected) exposing live reads and actions on the bots'
  `Player*` objects, marshalled onto the world-update thread. It also hooks party
  chat: messages with a command prefix (`!…`) are pushed to the C# service.
- **`service/`** — .NET 8 ASP.NET Core service. Holds all the LLM logic
  (provider-agnostic, native function-calling), decides which read/action tool to
  invoke, calls the module over HTTP, and asks the module to post a natural-language
  acknowledgement into party chat.

## v1 scope

- **Reads:** gold, level, inventory (live).
- **Action:** *buy item X from a nearby vendor* — resolve item by name, find the
  nearest vendor on the bot's current map (precomputed `bot_agent_item_vendors`
  table), travel there, buy, report the result back asynchronously.

## Data flow (example)

```
Player (party):  "!Thrall buy me a healing potion"
  -> [module] OnChat detects prefix, POST {service}/incoming { player, group, message, roster }
  -> [service] LLM tool-call: buy_item_from_vendor(bot=Thrall, item="...")
  -> POST {module}/bot/{guid}/buy  ->  { accepted, request_id, vendor }   (returns immediately)
  -> [service] optional interim ack -> POST {module}/chat "On my way to Goldshire..."
  ... bot travels across many world ticks, buys ...
  -> [module] POST {service}/action_result { request_id, success, item, price, ... }
  -> [service] final LLM ack -> POST {module}/chat "Got the potion, 2s50c well spent."
```

## Requirements

- AzerothCore (WotLK 3.3.5a) with **mod-playerbots** — the module links against
  mod-playerbots' `PlayerbotAI`/travel API for the buy-action movement.
- .NET 8 SDK for the service.
- An LLM provider key (Anthropic or OpenAI) for the service.

> Setup, build, and configuration details are documented as the implementation lands
> (see `module/conf/mod_bot_agent.conf.dist` and `service/appsettings.json`).

## License

GNU AGPL v3, same as AzerothCore.
