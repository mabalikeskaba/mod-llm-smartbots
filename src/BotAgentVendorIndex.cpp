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

    std::vector<VendorCandidate> FindNearestNpcWithFlag(
        uint16 map, float x, float y, float z,
        uint32 npcFlagMask, uint32 maxDistYards, uint32 limit)
    {
        std::vector<VendorCandidate> out;

        // Match spawns whose template carries all requested npcflag bits
        // (0x80 = UNIT_NPC_FLAG_VENDOR, 0x1000 = UNIT_NPC_FLAG_REPAIR).
        QueryResult result = WorldDatabase.Query(
            "SELECT c.id1, c.guid, c.position_x, c.position_y, c.position_z, ct.faction "
            "FROM creature c JOIN creature_template ct ON ct.entry = c.id1 "
            "WHERE c.map = {} AND (ct.npcflag & {}) = {} "
            "ORDER BY ((c.position_x-{})*(c.position_x-{})"
            "+(c.position_y-{})*(c.position_y-{})"
            "+(c.position_z-{})*(c.position_z-{})) ASC "
            "LIMIT {}",
            map, npcFlagMask, npcFlagMask, x, x, y, y, z, z, limit);

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
        return FindNearestNpcWithFlag(map, x, y, z, 0x80, maxDistYards, limit);
    }
}
