#!/usr/bin/env python3
"""
Discord Bridge Bot for F-DDrace.

Runs as a separate process (spawned by CDiscordBridge). Keeps the Discord
Gateway out of the game server so the engine tick is not blocked by discord.py.
"""

from __future__ import annotations

import discord
import json
import os
import re
import sys
import time
from typing import Optional
from urllib.parse import urlparse

import requests

print(f"Working directory: {os.getcwd()}")
print(f"Script location: {os.path.dirname(os.path.abspath(__file__))}")
sys.stdout.flush()

DISCORD_TOKEN = os.getenv("DISCORD_BOT_TOKEN", "").strip()
# Fallback when we can't resolve channel_id from sv_webhook_chat_url.
DEFAULT_CHANNEL_ID = "1480401568695193640"
DISCORD_CHANNEL_ID = os.getenv("DISCORD_CHANNEL_ID", "").strip()
SERVER_URL = os.getenv("SERVER_URL", "http://127.0.0.1:7778/discord")


def _load_sv_webhook_chat_url_from_autoexec() -> str:
    """
    Reads `sv_webhook_chat_url "https://discord.com/api/webhooks/.../..."`
    from `autoexec_server.cfg` so we can resolve the real Discord `channel_id`.
    """
    script_dir = os.path.dirname(os.path.abspath(__file__))

    candidates = [
        os.path.join(os.getcwd(), "autoexec_server.cfg"),
        os.path.join(script_dir, "autoexec_server.cfg"),
        os.path.join(os.getcwd(), "data", "autoexec_server.cfg"),
        os.path.join(script_dir, "data", "autoexec_server.cfg"),
    ]

    dir_path = script_dir
    for _ in range(6):
        parent = os.path.dirname(dir_path)
        if parent == dir_path:
            break
        dir_path = parent
        candidates.append(os.path.join(dir_path, "autoexec_server.cfg"))
        candidates.append(os.path.join(dir_path, "data", "autoexec_server.cfg"))

    pattern = re.compile(r'^\s*sv_webhook_chat_url\s+"([^"]+)"\s*$', re.IGNORECASE)
    for path in candidates:
        if not os.path.isfile(path):
            continue
        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as f:
                for line in f:
                    m = pattern.match(line)
                    if m:
                        return m.group(1).strip()
        except Exception:
            continue
    return ""


def _fetch_channel_id_from_webhook(webhook_url: str) -> Optional[int]:
    """
    Discord webhook URL (including token) can be called to retrieve webhook info,
    which includes `channel_id`.
    """
    try:
        r = requests.get(webhook_url, timeout=5)
        if r.status_code != 200:
            return None
        data = r.json()
        channel_id = data.get("channel_id")
        if channel_id is None:
            return None
        return int(channel_id)
    except Exception:
        return None


# Resolve CHANNEL_ID:
# 1) DISCORD_CHANNEL_ID env var (if set)
# 2) channel_id from sv_webhook_chat_url in autoexec_server.cfg
# 3) DEFAULT_CHANNEL_ID fallback
if not DISCORD_CHANNEL_ID or DISCORD_CHANNEL_ID == "0":
    webhook_chat_url = _load_sv_webhook_chat_url_from_autoexec()
    if webhook_chat_url:
        resolved = _fetch_channel_id_from_webhook(webhook_chat_url)
        if resolved is not None:
            DISCORD_CHANNEL_ID = str(resolved)

CHANNEL_ID = int(DISCORD_CHANNEL_ID or DEFAULT_CHANNEL_ID or "0")

# Optional local secret file (one line), so the token is not committed in source.
def _load_token_file() -> str:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [os.path.join(script_dir, "discord_bot.token")]
    # CMake copies discord_bot.py into the build dir; token usually lives in repo root.
    dir_path = script_dir
    for _ in range(6):
        parent = os.path.dirname(dir_path)
        if parent == dir_path:
            break
        dir_path = parent
        candidates.append(os.path.join(dir_path, "discord_bot.token"))
    for path in candidates:
        if os.path.isfile(path):
            with open(path, "r", encoding="utf-8") as f:
                token = f.read().strip()
            if token:
                return token
    return ""

if not DISCORD_TOKEN:
    DISCORD_TOKEN = _load_token_file()

intents = discord.Intents.default()
intents.message_content = True
client = discord.Client(intents=intents)

DISCORD_USER_TTL = 30 * 60
discord_users: dict[str, tuple[int, float]] = {}
_recent_events: dict[str, float] = {}
EVENT_DEDUP_SEC = 3.0

IMAGE_EXT = {".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp"}
VIDEO_EXT = {".mp4", ".webm", ".mov", ".mkv", ".avi"}
AUDIO_EXT = {".mp3", ".ogg", ".wav", ".flac", ".m4a", ".aac", ".opus"}


def register_user(name: str, user_id: int) -> None:
    discord_users[name.lower()] = (user_id, time.time())


def is_duplicate_event(key: str) -> bool:
    now = time.time()
    prev = _recent_events.get(key)
    if prev is not None and now - prev < EVENT_DEDUP_SEC:
        return True
    _recent_events[key] = now
    if len(_recent_events) > 256:
        cutoff = now - EVENT_DEDUP_SEC
        for k in list(_recent_events.keys()):
            if _recent_events[k] < cutoff:
                del _recent_events[k]
    return False


def get_user_id(name: str) -> Optional[int]:
    entry = discord_users.get(name.lower())
    if entry is None:
        return None
    user_id, last_seen = entry
    if time.time() - last_seen > DISCORD_USER_TTL:
        del discord_users[name.lower()]
        return None
    return user_id


def _ext(name: str) -> str:
    dot = name.rfind(".")
    return name[dot:].lower() if dot >= 0 else ""


def classify_attachment(att: discord.Attachment) -> str:
    ct = (att.content_type or "").lower()
    name = att.filename or ""
    ext = _ext(name)

    is_spoiler = getattr(att, "is_spoiler", False)
    if callable(is_spoiler):
        is_spoiler = is_spoiler()
    if "spoiler" in name.lower() or is_spoiler:
        tag = "spoiler"
    else:
        tag = ""

    if ct.startswith("image/") or ext in IMAGE_EXT:
        kind = "gif" if (ct.endswith("gif") or ext == ".gif") else "photo"
    elif ct.startswith("video/") or ext in VIDEO_EXT:
        kind = "video"
    elif ct.startswith("audio/") or ext in AUDIO_EXT:
        kind = "audio"
    else:
        kind = "file"

    if tag:
        return f"[{tag} {kind}]"
    return f"[{kind}]"


def classify_embed(embed: discord.Embed) -> list[str]:
    tags: list[str] = []
    etype = (embed.type or "").lower()

    if embed.video:
        tags.append("[video]")
    elif embed.image or etype == "image":
        tags.append("[photo]")
    elif embed.thumbnail and etype in ("gifv", "rich"):
        # Tenor / Giphy style embeds
        if etype == "gifv":
            tags.append("[gif]")
        else:
            tags.append("[embed]")

    if embed.url and not tags:
        host = urlparse(embed.url).netloc.lower()
        if any(x in host for x in ("youtube.", "youtu.be", "music.yandex", "soundcloud", "spotify", "open.spotify")):
            tags.append("[music]" if any(x in host for x in ("music", "soundcloud", "spotify")) else "[video]")
        else:
            tags.append("[link]")

    if embed.title and not tags:
        tags.append("[embed]")
    return tags


def format_media(message: discord.Message) -> str:
    parts: list[str] = []

    for att in message.attachments:
        parts.append(classify_attachment(att))

    for sticker in message.stickers:
        name = sticker.name or "sticker"
        parts.append(f"[sticker:{name}]")

    for embed in message.embeds:
        parts.extend(classify_embed(embed))

    # Collapse duplicates while preserving order: [photo] [photo] -> [photo x2]
    if not parts:
        return ""

    collapsed: list[str] = []
    i = 0
    while i < len(parts):
        tag = parts[i]
        count = 1
        while i + count < len(parts) and parts[i + count] == tag:
            count += 1
        if count > 1:
            # [photo] -> [photo x3]
            collapsed.append(tag[:-1] + f" x{count}]")
        else:
            collapsed.append(tag)
        i += count
    return " ".join(collapsed)


def build_message_text(message: discord.Message) -> str:
    chunks: list[str] = []
    content = (message.content or "").strip()
    if content:
        chunks.append(content)

    media = format_media(message)
    if media:
        chunks.append(media)

    # Discord "forwarded" / activity invites sometimes only have system content
    if not chunks and message.activity:
        chunks.append("[invite]")

    return " ".join(chunks).strip()


def display_name(user) -> str:
    if user is None:
        return "someone"
    return getattr(user, "display_name", None) or getattr(user, "name", None) or "someone"


def strip_game_level_suffix(name: str) -> str:
    # Game webhook names: "MishkaTeva [0]", "MishkaTeva [-5]"
    name = re.sub(r"\s\[-?\d+\]$", "", name).strip()
    if name.startswith("@"):
        name = name[1:]
    return name


def is_game_webhook_message(msg: Optional[discord.Message]) -> bool:
    # webhook_id lives on the Message, not on msg.author
    return msg is not None and getattr(msg, "webhook_id", None) is not None


def game_webhook_label(msg: discord.Message) -> str:
    name = display_name(msg.author) if msg.author else "player"
    if name.startswith("[Server]"):
        return "[Server]"
    return strip_game_level_suffix(name) or "player"


def discord_mention(user) -> str:
    """Discord participant — @login (user.name), not display/global name."""
    if user is None:
        return "@unknown"
    name = getattr(user, "name", None) or "unknown"
    return f"@{name}"


def author_mention_from_message(msg: Optional[discord.Message]) -> str:
    if msg is None:
        return "unknown"
    if is_game_webhook_message(msg):
        return game_webhook_label(msg)
    if msg.author:
        return discord_mention(msg.author)
    return "unknown"


def author_mention(user) -> str:
    """Message author when only the User object is available."""
    if user is None:
        return "unknown"
    return discord_mention(user)


def build_reaction_text(reactor_id: int, msg: Optional[discord.Message], emoji: str) -> str:
    preview = ""
    target = ""
    self_react = False
    if msg and msg.author:
        target = author_mention_from_message(msg)
        self_react = not is_game_webhook_message(msg) and reactor_id == msg.author.id
        preview = (msg.content or "").strip()
        if not preview:
            preview = format_media(msg)
        preview = re.sub(r"\s+", " ", preview)
        if len(preview) > 36:
            preview = preview[:33] + "..."

    if self_react:
        return f"{emoji} «{preview}»" if preview else emoji
    if preview:
        return f"{emoji} {target} · «{preview}»"
    return f"{emoji} {target}"


def emoji_display(emoji) -> str:
    """TeeWorlds can't render custom Discord emojis — use :name: for those."""
    if emoji is None:
        return "?"
    if getattr(emoji, "id", None):
        name = getattr(emoji, "name", None) or "emoji"
        return f":{name}:"
    return str(emoji)


def post_to_server(payload: dict) -> None:
    response = requests.post(
        SERVER_URL,
        data=json.dumps(payload, ensure_ascii=False).encode("utf-8"),
        headers={"Content-Type": "application/json; charset=utf-8"},
        timeout=5,
    )
    if response.status_code == 200:
        print(f"[{payload.get('event', 'message')}] @{payload.get('username')}: {payload.get('message')}")
    else:
        print(f"Send error: {response.status_code}")


@client.event
async def on_ready():
    print(f"Discord bot online as {client.user}")
    print(f"Channel ID: {CHANNEL_ID}")
    print(f"Server: {SERVER_URL}")


@client.event
async def on_message(message: discord.Message):
    if message.webhook_id:
        return
    if message.author.bot:
        return
    if message.channel.id != CHANNEL_ID:
        return

    register_user(message.author.name, message.author.id)

    try:
        message_text = build_message_text(message)
        if not message_text:
            return

        avatar_url = str(message.author.display_avatar.url) if message.author.display_avatar else ""
        reply_to = ""
        if message.reference:
            ref = message.reference.resolved
            if ref is None:
                try:
                    ref = await message.channel.fetch_message(message.reference.message_id)
                except Exception:
                    ref = None
            if ref is not None and hasattr(ref, "author") and ref.author:
                reply_to = author_mention_from_message(ref) if isinstance(ref, discord.Message) else author_mention(ref.author)

        payload = {
            "event": "chat",
            "username": message.author.name,
            "user_id": str(message.author.id),
            "message": message_text,
            "avatar_url": avatar_url,
            "reply_to": reply_to,
        }
        dedup = f"chat:{message.author.id}:{message.id}"
        if is_duplicate_event(dedup):
            return
        post_to_server(payload)
    except Exception as e:
        print(f"Error: {e}")


@client.event
async def on_raw_reaction_add(payload: discord.RawReactionActionEvent):
    if payload.channel_id != CHANNEL_ID:
        return
    if client.user and payload.user_id == client.user.id:
        return

    try:
        channel = client.get_channel(payload.channel_id)
        if channel is None:
            channel = await client.fetch_channel(payload.channel_id)

        user = payload.member
        if user is None:
            user = channel.guild.get_member(payload.user_id) if getattr(channel, "guild", None) else None
        if user is None:
            user = await client.fetch_user(payload.user_id)
        if user is None or getattr(user, "bot", False):
            return

        register_user(user.name, user.id)

        emoji = emoji_display(payload.emoji)
        dedup_key = f"react:{payload.user_id}:{payload.message_id}:{emoji}"
        if is_duplicate_event(dedup_key):
            return

        try:
            msg = await channel.fetch_message(payload.message_id)
        except Exception:
            msg = None

        text = build_reaction_text(payload.user_id, msg, emoji)

        avatar_url = ""
        if getattr(user, "display_avatar", None):
            avatar_url = str(user.display_avatar.url)

        payload_out = {
            "event": "reaction",
            "username": user.name,
            "user_id": str(user.id),
            "message": text,
            "avatar_url": avatar_url,
            "reply_to": "",
        }
        post_to_server(payload_out)
    except Exception as e:
        print(f"Reaction error: {e}")


if __name__ == "__main__":
    if not DISCORD_TOKEN:
        print("ERROR: set DISCORD_BOT_TOKEN in the environment")
        sys.exit(1)
    if CHANNEL_ID == 0:
        print("ERROR: DISCORD_CHANNEL_ID is empty/0 and sv_webhook_chat_url could not be resolved")
        sys.exit(1)

    print("Starting Discord bot...")
    client.run(DISCORD_TOKEN)
