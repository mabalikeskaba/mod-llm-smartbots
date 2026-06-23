#include "BotAgentChat.h"
#include "BotAgentJson.h"

#include "Chat.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "WorldPacket.h"

// mod-playerbots — to prefer a bot as the speaker.
#include "Playerbots.h"

namespace BotAgentChat
{
    std::string SendPartyMessage(uint32 groupLowId, std::string const& text)
    {
        if (text.empty())
            return BotAgentJson::Error("empty_text");

        Group* group = sGroupMgr->GetGroupByGUID(groupLowId);
        if (!group)
            return BotAgentJson::Error("group_not_found");

        // Prefer a bot as the speaker; fall back to any online member.
        Player* speaker = nullptr;
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member)
                continue;
            if (!speaker)
                speaker = member;
            if (GET_PLAYERBOT_AI(member))
            {
                speaker = member;
                break;
            }
        }

        if (!speaker)
            return BotAgentJson::Error("no_group_member");

        WorldPacket data;
        ChatHandler::BuildChatPacket(
            data,
            CHAT_MSG_PARTY,
            text,
            LANG_UNIVERSAL,
            CHAT_TAG_NONE,
            speaker->GetGUID(),
            speaker->GetName());

        int subGroup = -1;
        if (group->isRaidGroup())
            subGroup = group->GetMemberGroup(speaker->GetGUID());

        group->BroadcastPacket(&data, false, subGroup);

        return R"({"sent":true})";
    }
}
