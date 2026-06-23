#ifndef MOD_BOT_AGENT_BOTS_H
#define MOD_BOT_AGENT_BOTS_H

#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"

// Shared helper: resolve an in-world Player by its low GUID (the counter the
// C# service passes around). Returns nullptr if the player is not currently in
// the world. Must be called on the world thread.
namespace BotAgent
{
    inline Player* FindBotPlayer(uint32 guidLow)
    {
        ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(guidLow);
        return ObjectAccessor::FindPlayer(guid);
    }
}

#endif // MOD_BOT_AGENT_BOTS_H
