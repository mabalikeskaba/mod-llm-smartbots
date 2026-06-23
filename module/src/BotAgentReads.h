#ifndef MOD_BOT_AGENT_READS_H
#define MOD_BOT_AGENT_READS_H

#include "Define.h"
#include <string>

// Live reads of a bot's state. Every function here MUST be called on the
// world-update thread (i.e. from inside a BotAgentTaskQueue task), because it
// dereferences the live Player object. Each returns a JSON body string; on a
// missing/invalid bot it returns a JSON error object.
namespace BotAgentReads
{
    std::string Gold(uint32 botGuidLow);       // {"copper":N}
    std::string Level(uint32 botGuidLow);      // {"level":N}
    std::string Inventory(uint32 botGuidLow);  // {"items":[{entry,name,quality,count}]}
}

#endif // MOD_BOT_AGENT_READS_H
