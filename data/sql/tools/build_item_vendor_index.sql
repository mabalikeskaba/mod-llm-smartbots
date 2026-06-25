-- mod-bot-agent: (re)build the item -> vendor spawn index.
--
-- Run against the WORLD database (acore_world) AFTER the world import and after
-- any change to npc_vendor / creature. Idempotent: it rebuilds from scratch.
--
--   mysql -uroot -p<pw> acore_world < build_item_vendor_index.sql
--
-- Source tables:
--   npc_vendor        (entry = creature_template.entry, item = item_template.entry)
--   creature          (spawn rows: guid, id1/id2/id3 = template, map, position_*)
--   creature_template (faction)
--
-- NOTE on schema variants: modern AzerothCore stores the spawned template in
-- creature.id1/id2/id3. If your core still uses a single creature.id column,
-- replace "nv.entry IN (c.id1, c.id2, c.id3)" with "nv.entry = c.id".

TRUNCATE TABLE `bot_agent_item_vendors`;

-- INSERT IGNORE: npc_vendor may list the same item for a vendor more than once
-- (e.g. distinct ExtendedCost rows), which would collide on the
-- (item_entry, vendor_spawn_guid) primary key. We only need one row per pair.
INSERT IGNORE INTO `bot_agent_item_vendors`
    (`item_entry`, `vendor_entry`, `vendor_spawn_guid`, `map`,
     `position_x`, `position_y`, `position_z`, `faction`)
SELECT
    nv.`item`        AS item_entry,
    nv.`entry`       AS vendor_entry,
    c.`guid`         AS vendor_spawn_guid,
    c.`map`          AS map,
    c.`position_x`   AS position_x,
    c.`position_y`   AS position_y,
    c.`position_z`   AS position_z,
    ct.`faction`     AS faction
FROM `npc_vendor` nv
JOIN `creature` c
    ON nv.`entry` IN (c.`id1`, c.`id2`, c.`id3`)
JOIN `creature_template` ct
    ON ct.`entry` = nv.`entry`
WHERE nv.`item` > 0;
