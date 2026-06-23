# mod-bot-agent — AzerothCore module
#
# AzerothCore automatically collects `modules/<name>/src/*.cpp`. This file only
# adds the extra include directories the module needs:
#   1. the vendored single-header HTTP library (deps/httplib.h), and
#   2. mod-playerbots' headers (PlayerbotAI, TravelMgr, ...) used by the
#      buy-action's movement/strategy control.
#
# The module is built into AzerothCore's `modules` target. If you keep
# mod-playerbots at a non-default location, adjust MOD_PLAYERBOTS_SRC below.

# --- vendored HTTP library ---------------------------------------------------
target_include_directories(modules
  PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/deps")

# --- mod-playerbots headers (required for the buy-action) --------------------
set(MOD_PLAYERBOTS_SRC "${CMAKE_SOURCE_DIR}/modules/mod-playerbots/src"
    CACHE PATH "Path to the mod-playerbots module source tree")

if (EXISTS "${MOD_PLAYERBOTS_SRC}")
  # CollectIncludeDirectories is provided by AzerothCore's CMake macros.
  CollectIncludeDirectories(
    "${MOD_PLAYERBOTS_SRC}"
    BOT_AGENT_PLAYERBOTS_INCLUDES)
  target_include_directories(modules
    PRIVATE
      ${BOT_AGENT_PLAYERBOTS_INCLUDES})
  message(STATUS "mod-bot-agent: using mod-playerbots includes from ${MOD_PLAYERBOTS_SRC}")
else()
  message(WARNING
    "mod-bot-agent: mod-playerbots sources not found at ${MOD_PLAYERBOTS_SRC}. "
    "Reads/HTTP work, but the buy-action movement will fail to compile until "
    "mod-playerbots is present.")
endif()
