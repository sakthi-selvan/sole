# Overlay Chat

Always-on-top transparent C++ chatbot overlay for Linux (X11 / XWayland). Opacity slider and Hide/Show stay on screen in the bottom-right corner.

This is a normal desktop utility. It does **not** hide from other apps, spoof a system process name, or otherwise try to be undetectable.

## Build

```bash
make
make install
```

The binary installs to `~/.local/bin/overlay-chat`.

## API key

```bash
mkdir -p ~/.config/overlay-chat
printf 'NVIDIA_API_KEY=your-key-here\n' > ~/.config/overlay-chat/env
chmod 600 ~/.config/overlay-chat/env
```

Or export `NVIDIA_API_KEY` in the environment. Endpoint and model match the NVIDIA integrate API (`deepseek-ai/deepseek-v4-flash-0731`).

## START

```bash
overlay-chat
```

or

```bash
./scripts/start.sh
```

or

```bash
NVIDIA_API_KEY=your-key-here ./overlay-chat
```

## END

```bash
overlay-chat --quit
```

or

```bash
./scripts/stop.sh
```

Status: `overlay-chat --status`

## Run at login

```bash
make autostart
```

That writes `~/.config/autostart/overlay-chat.desktop` so the overlay starts after you log in. It will still appear as `overlay-chat` in the process list.

## Controls

- Opacity slider (always visible): drag low → high
- **Hide** / **Show** (bottom-right of the overlay): collapses the chat; slider + button stay
- Enter or **Send** to chat
- Escape collapses the chat
- Drag the title bar to move
- `Ctrl+Q` while focused also ends the app
