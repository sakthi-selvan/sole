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

Put the key in **`.env`** (copy from `.env.example`) or in `~/.config/overlay-chat/env`:

```bash
cp .env.example .env
# edit .env and set NVIDIA_API_KEY=...

# or:
cp .env.example ~/.config/overlay-chat/env
chmod 600 ~/.config/overlay-chat/env
```

Environment variables still override the files. Endpoint and model match the NVIDIA integrate API (`deepseek-ai/deepseek-v4-flash-0731`).

## START

```bash
overlay-chat
```

It detaches immediately and keeps running after you close the terminal, until `overlay-chat --quit` or the machine powers off.

```bash
./scripts/start.sh
```

Stay attached to the terminal (debug): `overlay-chat --foreground`

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

- Opacity slider: drag low → high. At 0% the overlay stays slightly visible
- **Type** / **OK**: click Type so typing goes into the message; click OK to stop
- **Ctrl+Shift+S**: fully hide or show the overlay (GNOME shortcut + in-app)
- Enter or **Send** to chat
- Escape collapses the chat
- Drag the title bar to move
- `Ctrl+Q` while focused also ends the app
