#include "BotAgentVendorIndex.h"

#include "DatabaseEnv.h"
#include "Field.h"
#include "QueryResult.h"

#include <cmath>

namespace BotAgentVendorIndex
{
    std::vector<VendorCandidate> FindNearest(
        uint32 itemEntry, uint16 map,
        float x, float y, float z,
        uint32 maxDistYards, uint32 limit)
    {
        std::vector<VendorCandidate> out;

        // Nearest-first by squared distance (no sqrt needed for ordering).
        QueryResult result = WorldDatabase.Query(
            "SELECT vendor_entry, vendor_spawn_guid, position_x, position_y, position_z, faction "
            "FROM bot_agent_item_vendors "
            "WHERE item_entry = {} AND map = {} "
            "ORDER BY ((position_x-{})*(position_x-{})"
            "+(position_y-{})*(position_y-{})"
            "+(position_z-{})*(position_z-{})) ASC "
            "LIMIT {}",
            itemEntry, map, x, x, y, y, z, z, limit);

        if (!result)
            return out;

        do
        {
            Field* fields = result->Fetch();
            VendorCandidate c;
            c.vendorEntry = fields[0].Get<uint32>();
            c.spawnGuid   = fields[1].Get<uint32>();
            c.x           = fields[2].Get<float>();
            c.y           = fields[3].Get<float>();
            c.z           = fields[4].Get<float>();
            c.faction     = fields[5].Get<uint32>();
            c.map         = map;

            float dx = c.x - x, dy = c.y - y, dz = c.z - z;
            c.distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (maxDistYards == 0 || c.distance <= static_cast<float>(maxDistYards))
                out.push_back(c);
        }
        while (result->NextRow());

        return out;
    }

    std::vector<VendorCandidate> FindNearestAnyVendor(
        uint16 map, float x, float y, float z,
        uint32 maxDistYards, uint32 limit)
    {
        std::vector<VendorCandidate> out;

        // 0x80 == UNIT_NPC_FLAG_VENDOR. Any vendor buys items, so we only need
        // the nearest vendor-flagged spawn on the bot's map.
        QueryResult result = WorldDatabase.Query(
            "SELECT c.id1, c.guid, c.position_x, c.position_y, c.position_z, ct.faction "
            "FROM creature c JOIN creature_template ct ON ct.entry = c.id1 "
            "WHERE c.map = {} AND (ct.npcflag & 0x80) <> 0 "
            "ORDER BY ((c.position_x-{})*(c.position_x-{})"
            "+(c.position_y-{})*(c.position_y-{})"
            "+(c.position_z-{})*(c.position_z-{})) ASC "
            "LIMIT {}",
            map, x, x, y, y, z, z, limit);

        if (!result)
            return out;

        do
        {
            Field* fields = result->Fetch();
            VendorCandidate c;
            c.vendorEntry = fields[0].Get<uint32>();
            c.spawnGuid   = fields[1].Get<uint32>();
            c.x           = fields[2].Get<float>();
            c.y           = fields[3].Get<float>();
            c.z           = fields[4].Get<float>();
            c.faction     = fields[5].Get<uint32>();
            c.map         = map;

            float dx = c.x - x, dy = c.y - y, dz = c.z - z;
            c.distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (maxDistYards == 0 || c.distance <= static_cast<float>(maxDistYards))
                out.push_back(c);
        }
        while (result->NextRow());

        return out;
    }
}
