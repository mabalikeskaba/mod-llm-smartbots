#ifndef MOD_BOT_AGENT_ACTION_COMMON_H
#define MOD_BOT_AGENT_ACTION_COMMON_H

#include "BotAgentConfig.h"
#include "BotAgentHttpClient.h"
#include "BotAgentJson.h"

#include "Bag.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Item.h"
#include "Player.h"
#include "QueryResult.h"

#include "PlayerbotAI.h"

#include <string>
#include <unordered_set>
#include <vector>

// Shared scaffolding for the async, world-thread-only bot actions (sell, trade).
// Mirrors the helpers the buy action keeps private, factored out so the newer
// actions don't duplicate them. All functions run on the WORLD THREAD only.
namespace BotAgentActionCommon
{
    inline std::string SqlEscape(std::string const& in)
    {
        std::string out;
        out.reserve(in.size());
        for (char c : in)
        {
            if (c == '\'')      out += "''";
            else if (c == '\\') {} // drop backslashes
            else                out += c;
        }
        return out;
    }

    struct ResolvedItem { uint32 entry = 0; std::string name; };

    // Resolve an item by name (exact, then substring) via the world DB.
    inline ResolvedItem ResolveItemByName(std::string const& name)
    {
        std::string esc = SqlEscape(name);

        QueryResult r = WorldDatabase.Query(
            "SELECT entry, name FROM item_template WHERE name = '{}' LIMIT 1", esc);
        if (!r)
            r = WorldDatabase.Query(
                "SELECT entry, name FROM item_template WHERE name LIKE '%{}%' LIMIT 1", esc);

        ResolvedItem out;
        if (r)
        {
            Field* f = r->Fetch();
            out.entry = f[0].Get<uint32>();
            out.name  = f[1].Get<std::string>();
        }
        return out;
    }

    // Splits a '|'-delimited item-name list (the wire format the C# service
    // uses, since the module's JSON helper only extracts flat strings) and
    // trims surrounding whitespace from each entry. Empty entries are dropped.
    inline std::vector<std::string> SplitItemList(std::string const& list)
    {
        std::vector<std::string> out;
        size_t start = 0;
        while (start <= list.size())
        {
            size_t bar = list.find('|', start);
            std::string token = list.substr(start, bar == std::string::npos ? std::string::npos : bar - start);

            size_t a = token.find_first_not_of(" \t");
            size_t b = token.find_last_not_of(" \t");
            if (a != std::string::npos)
                out.push_back(token.substr(a, b - a + 1));

            if (bar == std::string::npos)
                break;
            start = bar + 1;
        }
        return out;
    }

    // Collect every Item* in the bot's backpack and equipped bags whose entry is
    // in `wanted`. Order: backpack first, then bags.
    inline std::vector<Item*> CollectItemsByEntry(Player* bot, std::unordered_set<uint32> const& wanted)
    {
        std::vector<Item*> found;

        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                if (wanted.count(item->GetEntry()))
                    found.push_back(item);

        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        {
            Bag* pBag = bot->GetBagByPos(bag);
            if (!pBag)
                continue;
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                if (Item* item = bot->GetItemByPos(bag, static_cast<uint8>(j)))
                    if (wanted.count(item->GetEntry()))
                        found.push_back(item);
        }
        return found;
    }

    inline std::vector<std::string> SuspendAI(PlayerbotAI* botAI)
    {
        std::vector<std::string> saved = botAI->GetStrategies(BOT_STATE_NON_COMBAT);
        botAI->ChangeStrategy("-follow,-grind,+passive", BOT_STATE_NON_COMBAT);
        return saved;
    }

    inline void RestoreAI(PlayerbotAI* botAI, std::vector<std::string> const& saved)
    {
        botAI->ClearStrategies(BOT_STATE_NON_COMBAT);
        for (std::string const& s : saved)
            botAI->ChangeStrategy("+" + s, BOT_STATE_NON_COMBAT);
    }

    // POST a generic action result to the C# service. The field shape matches
    // what /action_result expects: { request_id, success, item, price, vendor,
    // message }. Field meanings are reinterpreted per action kind on the service
    // side (it correlates request_id to the originating command).
    inline void PostResult(std::string const& requestId, bool success,
                           std::string const& item, uint32 priceCopper,
                           std::string const& vendor, std::string const& message)
    {
        std::string body = "{";
        body += "\"request_id\":\"" + BotAgentJson::Escape(requestId) + "\",";
        body += "\"success\":" + std::string(success ? "true" : "false") + ",";
        body += "\"item\":\"" + BotAgentJson::Escape(item) + "\",";
        body += "\"price\":" + std::to_string(priceCopper) + ",";
        body += "\"vendor\":\"" + BotAgentJson::Escape(vendor) + "\",";
        body += "\"message\":\"" + BotAgentJson::Escape(message) + "\"";
        body += "}";

        std::string url = BotAgentConfig::Instance().CallbackBaseUrl + "/action_result";
        BotAgentHttpClient::Instance().PostJson(url, std::move(body));
    }
}

#endif // MOD_BOT_AGENT_ACTION_COMMON_H
