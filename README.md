# M-DDrace on DDNet 20.3+

[![Build status](https://github.com/ddnet/ddnet/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/ddnet/ddnet/actions/workflows/build.yml?branch=master)
[![Code coverage](https://codecov.io/gh/ddnet/ddnet/branch/master/graph/badge.svg)](https://codecov.io/gh/ddnet/ddnet/branch/master)
[![Translation status](https://hosted.weblate.org/widget/ddnet/ddnet/svg-badge.svg)](https://hosted.weblate.org/engage/ddnet/)

**M-DDrace (Modern Edition)** is a next-generation port and complete overhaul of the popular open-world [F-DDrace](https://github.com/fokkonaut/F-DDrace) modification, re-architected on top of the latest **DDNet 20.3+** engine with modern **C++20**.

This project combines DDNet's official master server integration, updated physics, high performance, and tickrate reliability with the rich open-world gameplay, player housing, minigames, and comprehensive anti-cheat/anti-bot protection of M-DDrace.

---

## Mod Features

### 🛡️ Anti-Cheat & Network Hardening
- **DDNet Token & Binary Checksum Verification:** Authentic client validation using token handshakes and MD5/SHA checksums against `anticheat_hashes.cfg`. Blocks cracked clients, multi-press bots, and modified binaries while whitelisting official DDNet builds.
- **Packet Burst Suppression:** Deep network pre-filtering to prevent packet queue exhaustion and CPU thread starvation.
- **Secure Messaging:** Rate-limited private messaging (`/w`, `/whisper`, `/c`) preventing spam bypasses.
- **Discord Bot / Webhook Telemetry:** Automated reporting for antibot triggers, moderator actions, and minigame events directly to Discord channels.

### 🎮 Gameplay, Economy & Progression
- **Modular Architecture (`CONF_FDDRACE_MOD`):** Cleanly separated mod logic located in `src/mod/` and linked via `cmake/fddrace_mod.cmake`. Toggleable at compile-time with `-DFDDRACE_MOD=ON/OFF`.
- **Account & Leveling System:** Complete player profile persistence (XP, money, kills, deaths, playtime). Progressive 100-level scaling curve, double XP bonuses, and level-up rewards.
- **Dynamic Plots & Real-Time Ingame Editor:** Rent personal plots and construct custom creations inside the live server:
  - Laser walls (custom color, angle, thickness, collision).
  - Laser doors with paired activation buttons.
  - Directional speedups and jump pads.
  - Multi-mode teleporters (standard, weapon, evil red/blue).
  - Advanced selection, copy, move, and template tools.
- **Minigames Suite:**
  - **FNG (Freeze 'n Girl):** Fast-paced hammer and laser arena with solo and team scoring.
  - **Survival:** Battle-royale survival gamemode.
  - **1vs1 Arenas:** On-demand duel zones created anywhere on the map (`/1vs1 <player>`).
  - **Durak Card Game:** Fully integrated Russian card game playable directly on the map.
  - **Block Arenas:** Block battles with floating score indicators and bonus multipliers.
- **Killstreak & Spree Announcer:** Audio cues and global broadcast announcements for killstreaks (Killing Spree, Rampage, Relentless, Unstoppable, GODLIKE).

---

## Quickstart & Building

### Prerequisites
- CMake 3.17+
- C++20 compliant compiler (MSVC 2022 v143+ on Windows, GCC 11+ or Clang 13+ on Linux)
- Python 3.8+

### Windows (MSVC 2022)

The repository includes a ready-to-use build script:
```bat
_build_game_server.bat
```

Or configure and build manually via CMake:
```bat
cmake -B build -G "Visual Studio 17 2022" -A x64 -DFDDRACE_MOD=ON -DPREFER_BUNDLED_LIBS=ON -DCLIENT=OFF
cmake --build build --config Release --target M-DDrace-Server
```

### Linux / macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFDDRACE_MOD=ON -DCLIENT=OFF
cmake --build build --target M-DDrace-Server -j$(nproc)
```

The resulting server binary will be named **`M-DDrace-Server`** (`M-DDrace-Server.exe` on Windows).

---

## Server Configuration

Place your `autoexec.cfg` and `anticheat_hashes.cfg` in the server root or binary directory.

### Key Configuration Variables

```cfg
# Network & Anti-Cheat
sv_require_ddnet_checksum 0     # Set to 1 to enforce anticheat_hashes.cfg whitelist
sv_anticheat_mode 1             # Enable heuristic cheat analysis

# Persistence & Performance
sv_periodic_save 1              # Enable automated periodic save
sv_data_save_interval 5         # Save interval in minutes
sv_shutdown_save_tees 1         # Save players and plots on server shutdown

# Discord Webhooks
sv_discord_bridge_port "7778"
sv_webhook_antibot_url "https://discord.com/api/webhooks/..."
sv_webhook_minigames_url "https://discord.com/api/webhooks/..."
sv_webhook_chat_url "https://discord.com/api/webhooks/..."
```

---

## Upstream DDNet Reference

This repository is built upon [DDraceNetwork (DDNet)](https://ddnet.org), an open-source Teeworlds modification.

- Official DDNet Website: <https://ddnet.org>
- Source Code Documentation: <https://codedoc.ddnet.org/>
- Upstream Building Guide: [docs/BUILDING.md](docs/BUILDING.md)
- Development Wiki: <https://wiki.ddnet.org/wiki/Development>

To build upstream vanilla DDNet without M-DDrace mod features, simply pass `-DFDDRACE_MOD=OFF` to CMake.

---

## Credits & Attribution

- **M-DDrace Mod Developer & Modern DDNet Port:** [MishkaTeva](https://github.com/MishkaTeva)
- **F-DDrace Creator & Original Author:** [fokkonaut](https://github.com/fokkonaut)
- **DDraceNetwork Core Team:** [Dennis Felsing (heinrich5991)](https://github.com/heinrich5991) and the DDNet team
- **DDRace Creator:** Shereef Marzouk
- **Teeworlds Creator:** [Magnus Auvinen (matricks)](https://github.com/matricks) and the Teeworlds community

## License

- Source code is licensed under the **zlib/libpng** license.
- Assets under `data/` are licensed under **CC-BY-SA 3.0** (unless otherwise specified).
- See [`license.txt`](license.txt) for detailed licensing terms.
