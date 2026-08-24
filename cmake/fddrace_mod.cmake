# F-DDrace mod layer (server-side). DDNet engine/protocol/physics remain the base.

set(FDDRACE_MOD_ENGINE_SRC
  src/mod/engine/server/anticheat.cpp
  src/mod/engine/server/anticheat.h
  src/mod/engine/server/discord_bridge.cpp
  src/mod/engine/server/discord_bridge.h
  src/mod/engine/server/mod_server_integration.cpp
  src/mod/engine/server/mod_server_integration.h
)

set(FDDRACE_MOD_GAME_SRC
  src/mod/game/server/account.cpp
  src/mod/game/server/account.h
  src/mod/game/server/account_commands.cpp
  src/mod/game/server/versioncheck.cpp
  src/mod/game/server/versioncheck.h
  src/mod/game/server/entities/lasertext.cpp
  src/mod/game/server/entities/lasertext.h
  src/mod/game/server/entities/money.cpp
  src/mod/game/server/entities/money.h
  src/mod/game/server/entities/jail_arrest.cpp
  src/mod/game/server/entities/jail_arrest.h
  src/mod/game/server/entities/jail_release.cpp
  src/mod/game/server/entities/jail_release.h
)

set(FDDRACE_MOD_SRC
  ${FDDRACE_MOD_ENGINE_SRC}
  ${FDDRACE_MOD_GAME_SRC}
)

function(apply_fddrace_mod target)
  if(NOT FDDRACE_MOD)
    return()
  endif()
  target_compile_definitions(${target} PRIVATE CONF_FDDRACE_MOD)
  target_sources(${target} PRIVATE ${FDDRACE_MOD_SRC})
endfunction()
