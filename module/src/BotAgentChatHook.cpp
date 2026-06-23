#include "BotAgentChatHook.h"
#include "BotAgentConfig.h"
#include "BotAgentHttpClient.h"
#include "BotAgentJson.h"

#include "Group.h"
#include "Player.h"
#include "SharedDefines.h"

#include <string>

namespace
{
    bool IsPartyOrRaidChat(uint32 type)
    {
        switch (type)
        {
            case CHAT_MSG_PARTY:
            case CHAT_MSG_PARTY_LEADER:
            case CHAT_MSG_RAID:
            case CHAT_MSG_RAID_LEADER:
            case CHAT_MSG_RAID_WARNING:
                return true;
            default:
                return false;
        }
    }

    std::string LStrip(std::string const& s)
    {
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
            ++i;
        return s.substr(i);
    }
}

BotAgentChatHook::BotAgentChatHook()
    : PlayerScript("BotAgentChatHook", { PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE })
{
}

void BotAgentChatHook::OnPlayerBeforeSendChatMessage(
    Player* player, uint32& type, uint32& /*lang*/, std::string& msg)
{
    BotAgentConfig const& cfg = BotAgentConfig::Instance();
    if (!cfg.Enable || !player)
        return;

    if (!IsPartyOrRaidChat(type))
        return;

    std::string const& prefix = cfg.CommandPrefix;
    if (prefix.empty() || msg.rfind(prefix, 0) != 0)
        return; // not a command for us

    Group* group = player->GetGroup();
    if (!group)
        return;

    std::string command = LStrip(msg.substr(prefix.size()));
    if (command.empty())
        return;

    // Build the roster of the speaker's group companions (everyone but the
    // speaker). Touching members here is safe — we are on the world thread.
    ObjectGuid speakerGuid = player->GetGUID();
    std::string roster = "[";
    bool first = true;

    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || member->GetGUID() == speakerGuid)
            continue;

        if (!first)
            roster += ',';
        first = false;

        roster += "{\"guid\":";
        roster += std::to_string(member->GetGUID().GetCounter());
        roster += ",\"name\":\"";
        roster += BotAgentJson::Escape(member->GetName());
        roster += "\",\"class\":";
        roster += std::to_string(uint32(member->getClass()));
        roster += ",\"level\":";
        roster += std::to_string(uint32(member->GetLevel()));
        roster += ",\"map\":";
        roster += std::to_string(member->GetMapId());
        roster += '}';
    }
    roster += ']';

    // group_guid is the full raw GUID as a string (it can exceed 2^53, so it is
    // not safe to send as a JSON number); the C# service echoes it back and the
    // module resolves it to send the acknowledgement into this party.
    std::string body = "{";
    body += "\"player\":\"" + BotAgentJson::Escape(player->GetName()) + "\",";
    body += "\"player_guid\":" + std::to_string(speakerGuid.GetCounter()) + ",";
    body += "\"group_guid\":\"" + std::to_string(group->GetGUID().GetRawValue()) + "\",";
    body += "\"message\":\"" + BotAgentJson::Escape(command) + "\",";
    body += "\"roster\":" + roster;
    body += "}";

    BotAgentHttpClient::Instance().PostJson(cfg.IncomingUrl, std::move(body));
}
