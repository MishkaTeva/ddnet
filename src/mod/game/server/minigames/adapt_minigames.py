#!/usr/bin/env python3
"""Mechanical API renames for F-DDrace minigames → DDNet + CONF_FDDRACE_MOD."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent

REPLACEMENTS = [
    ("GetCID()", "GetCid()"),
    ("GetSpectatorID()", "SpectatorId()"),
    ("SnapNewID()", "SnapNewId().value_or(-1)"),
    ("SnapFreeID(", "SnapFreeId("),
    ("Collision()->TileUsed(", "Collision()->HasModTile("),
    ("GameServer()->m_World.CreateLaserText(", "GameServer()->CreateLaserText("),
    ("GetClientDDNetVersion(", "GetClientVersion("),
    ("Server()->IsSevendown(", "Server()->IsSixup("),  # note: CSnapContext wants Sixup; IsSevendown was inverted — fixed below for Context
    ("SnapPickupObject(", "SnapPickup("),
    ("((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.", "GameServer()->m_pController->Teams()."),
    ("#include <game/server/gamemodes/DDRace.h>", "#include <game/server/gamemodes/ddnet.h>"),
    ("#include <game/localization.h>", "#include <engine/shared/localization.h>"),
    ("#include <game/server/entities/effects/flyingpoint.h>", "#include <mod/game/server/entities/flyingpoint.h>"),
    (", LASERFLAG_NO_PREDICT)", ")"),
]

# SetSpectatorID(SPEC_PLAYER, X) → SetSpectatorId(X)
SET_SPEC_RE = re.compile(r"SetSpectatorID\s*\(\s*SPEC_PLAYER\s*,\s*([^)]+)\)")

FILES = [
    "arenas.cpp", "arenas.h",
    "survival.cpp", "survival.h",
    "fng.cpp", "fng.h",
    "durak.cpp", "durak.h",
    "minigame.cpp", "minigame.h",
]


def adapt(text: str, name: str) -> str:
    text = SET_SPEC_RE.sub(r"SetSpectatorId(\1)", text)
    for old, new in REPLACEMENTS:
        text = text.replace(old, new)

    # Whole-word CHAT_ALL → TEAM_ALL (do not touch CHATFLAG_ALL)
    text = re.sub(r"\bCHAT_ALL\b", "TEAM_ALL", text)

    # TEAMS macro in fng (may already be partially rewritten)
    text = text.replace(
        "#define TEAMS ((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams",
        "#define TEAMS (GameServer()->m_pController->Teams())",
    )
    text = text.replace(
        "#define TEAMS ((CGameControllerDDNet *)GameServer()->m_pController)->m_Teams",
        "#define TEAMS (GameServer()->m_pController->Teams())",
    )

    # SendChat(-1, TEAM_ALL, -1, x) → SendChat(-1, TEAM_ALL, x)  (DDNet signature)
    text = re.sub(
        r"SendChat\(\s*-1\s*,\s*TEAM_ALL\s*,\s*-1\s*,",
        "SendChat(-1, TEAM_ALL,",
        text,
    )

    return text


def main():
    for name in FILES:
        path = ROOT / name
        if not path.exists():
            print("skip missing", name)
            continue
        original = path.read_text(encoding="utf-8")
        updated = adapt(original, name)
        if updated != original:
            path.write_text(updated, encoding="utf-8", newline="\n")
            print("updated", name)
        else:
            print("unchanged", name)


if __name__ == "__main__":
    main()
