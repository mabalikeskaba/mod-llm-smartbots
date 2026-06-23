#ifndef MOD_BOT_AGENT_VENDOR_INDEX_H
#define MOD_BOT_AGENT_VENDOR_INDEX_H

#include "Define.h"
#include <vector>

// Reads the precomputed bot_agent_item_vendors table (world DB) to find vendor
// spawns that sell a given item on a given map, nearest first. Faction
// reaction and reachability are decided by the caller (which has the bot).
namespace BotAgentVendorIndex
{
    struct VendorCandidate
    {
        uint32 vendorEntry = 0;
        uint32 spawnGuid   = 0;
        uint16 map         = 0;
        float  x           = 0.0f;
        float  y           = 0.0f;
        float  z           = 0.0f;
        uint32 faction     = 0;
        float  distance    = 0.0f; // yards from the query position
    };

    // Vendor spawns on `map` selling `itemEntry`, ordered nearest-first to
    // (x,y,z). `maxDistYards` == 0 means "no distance cap". `limit` caps rows.
    // Safe to call from any thread (pure DB read).
    std::vector<VendorCandidate> FindNearest(
        uint32 itemEntry, uint16 map,
        float x, float y, float z,
        uint32 maxDistYards, uint32 limit = 25);
}

#endif // MOD_BOT_AGENT_VENDOR_INDEX_H
