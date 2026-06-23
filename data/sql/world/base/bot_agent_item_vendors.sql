-- mod-bot-agent: item -> vendor spawn lookup table (world DB / acore_world)
--
-- Flattened index that maps each sellable item to every vendor SPAWN that
-- offers it, with the spawn's map and coordinates. Populated by
-- tools/build_item_vendor_index.sql after the world DB is imported.
--
-- One row per (item, vendor spawn): a vendor with multiple spawns contributes
-- multiple rows so the nearest one can be chosen at runtime.

CREATE TABLE IF NOT EXISTS `bot_agent_item_vendors` (
    `item_entry`        INT UNSIGNED      NOT NULL,
    `vendor_entry`      INT UNSIGNED      NOT NULL,             -- creature_template.entry
    `vendor_spawn_guid` INT UNSIGNED      NOT NULL,             -- creature.guid (specific spawn)
    `map`               SMALLINT UNSIGNED NOT NULL,
    `position_x`        FLOAT             NOT NULL,
    `position_y`        FLOAT             NOT NULL,
    `position_z`        FLOAT             NOT NULL,
    `faction`           SMALLINT UNSIGNED NOT NULL DEFAULT 0,   -- creature_template.faction
    PRIMARY KEY (`item_entry`, `vendor_spawn_guid`),
    KEY `idx_item_map` (`item_entry`, `map`)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;
