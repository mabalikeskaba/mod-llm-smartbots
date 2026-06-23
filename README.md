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
  mod-playerbots' `PlayerbotAI` API to control the bot during the buy action.
- .NET 8 SDK/runtime for the service.
- An LLM provider key (Anthropic or OpenAI) for the service.

## Setup

### 1. Build the C++ module

```bash
# from your AzerothCore source root
cd modules
git clone <this-repo> mod-bot-agent
# mod-playerbots must also be present under modules/ (the module includes its headers)
cd ..
# reconfigure + rebuild AzerothCore (CMake picks up modules/mod-bot-agent/src automatically)
```

The module's `CMakeLists.txt` adds the vendored `module/deps/httplib.h` and the
mod-playerbots headers to the build. If mod-playerbots lives at a non-default
path, pass `-DMOD_PLAYERBOTS_SRC=/path/to/mod-playerbots/src`.

### 2. Create and populate the vendor index (world DB)

```bash
# create the table
mysql -uroot -p<pw> acore_world < modules/mod-bot-agent/module/data/sql/world/base/bot_agent_item_vendors.sql
# (re)build it from npc_vendor + creature — rerun after any world DB update
mysql -uroot -p<pw> acore_world < modules/mod-bot-agent/module/data/sql/tools/build_item_vendor_index.sql
```

> If your core stores spawns in a single `creature.id` column (not `id1/id2/id3`),
> edit the join in `build_item_vendor_index.sql` as noted in its header.

### 3. Configure the module

Copy `module/conf/mod_bot_agent.conf.dist` to your worldserver config dir as
`mod_bot_agent.conf` and set at least:

- `LLMAgent.Http.Token` — a shared secret (must match the service).
- `LLMAgent.Http.Port` (default `8810`) and `LLMAgent.Service.IncomingUrl` /
  `LLMAgent.Service.CallbackBaseUrl` pointing at the C# service (default `8820`).

Start (or restart) worldserver. The bridge logs `HTTP server listening on …`.

### 4. Configure and run the C# service

Edit `service/BotAgent.Service/appsettings.json` (or use environment variables /
`appsettings.Local.json`, which is git-ignored):

- `Module.BaseUrl` = `http://127.0.0.1:8810`, `Module.Token` = the SAME token.
- `Llm.Provider` (`anthropic` | `openai`), `Llm.Model`, `Llm.ApiKey`.
- `Urls` = `http://127.0.0.1:8820` (matches the module's IncomingUrl/CallbackBaseUrl).

```bash
cd service
dotnet run --project BotAgent.Service        # listens on :8820
dotnet test                                  # run the unit tests
```

### 5. Use it in game

Group up with playerbots, then type a prefixed command in **party/raid** chat:

```
!Thrall how much gold do you have?
!Thrall buy me a healing potion
```

The bot acknowledges in party chat. A buy travels to the nearest vendor on the
bot's map and posts a follow-up acknowledgement when done.

## Verify quickly (without the LLM)

```bash
# reads (replace 42 with a bot's low GUID; send the token)
curl -H "X-Agent-Token: <token>" http://127.0.0.1:8810/bot/42/gold
curl -H "X-Agent-Token: <token>" http://127.0.0.1:8810/bot/42/inventory
# start a buy
curl -H "X-Agent-Token: <token>" -H "Content-Type: application/json" \
     -d '{"item_name":"Healing Potion"}' http://127.0.0.1:8810/bot/42/buy
```

## License

GNU AGPL v3, same as AzerothCore.
