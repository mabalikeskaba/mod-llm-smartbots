#include "BotAgentReads.h"
#include "BotAgentBots.h"
#include "BotAgentJson.h"

#include "Bag.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Player.h"

#include <string>

namespace
{
    // Appends one inventory item as a JSON object to `out`. `first` tracks
    // comma placement across the array.
    void AppendItemJson(std::string& out, bool& first, Item const* item)
    {
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            return;

        if (!first)
            out += ',';
        first = false;

        out += "{\"entry\":";
        out += std::to_string(proto->ItemId);
        out += ",\"name\":\"";
        out += BotAgentJson::Escape(proto->Name1);
        out += "\",\"quality\":";
        out += std::to_string(proto->Quality);
        out += ",\"count\":";
        out += std::to_string(item->GetCount());
        out += '}';
    }
}

std::string BotAgentReads::Gold(uint32 botGuidLow)
{
    Player* bot = BotAgent::FindBotPlayer(botGuidLow);
    if (!bot)
        return BotAgentJson::Error("bot_not_in_world");

    return std::string("{\"copper\":") + std::to_string(bot->GetMoney()) + "}";
}

std::string BotAgentReads::Level(uint32 botGuidLow)
{
    Player* bot = BotAgent::FindBotPlayer(botGuidLow);
    if (!bot)
        return BotAgentJson::Error("bot_not_in_world");

    return std::string("{\"level\":") + std::to_string(uint32(bot->GetLevel())) + "}";
}

std::string BotAgentReads::Inventory(uint32 botGuidLow)
{
    Player* bot = BotAgent::FindBotPlayer(botGuidLow);
    if (!bot)
        return BotAgentJson::Error("bot_not_in_world");

    std::string out = "{\"items\":[";
    bool first = true;

    // Main backpack.
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            AppendItemJson(out, first, item);

    // Equipped bags' contents.
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Bag* pBag = bot->GetBagByPos(bag);
        if (!pBag)
            continue;

        for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            if (Item* item = bot->GetItemByPos(bag, static_cast<uint8>(j)))
                AppendItemJson(out, first, item);
    }

    out += "]}";
    return out;
}
