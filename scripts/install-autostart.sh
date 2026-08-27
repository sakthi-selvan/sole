#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${HOME}/.local/bin/overlay-chat"
AUTOSTART_DIR="${HOME}/.config/autostart"
DESKTOP="${AUTOSTART_DIR}/overlay-chat.desktop"
ENVF="${HOME}/.config/overlay-chat/env"

mkdir -p "${HOME}/.local/bin" "${AUTOSTART_DIR}" "${HOME}/.config/overlay-chat"

if [[ ! -x "${ROOT}/overlay-chat" ]]; then
  echo "Build first: make -C ${ROOT}"
  exit 1
fi

cp "${ROOT}/overlay-chat" "${BIN}"
chmod 755 "${BIN}"

if [[ ! -f "${ENVF}" ]]; then
  cat > "${ENVF}" <<'EOF'
# GROQ_API_KEY=replace-me
# GROQ_API_BASE=https://api.groq.com/openai/v1
# GROQ_MODEL=openai/gpt-oss-20b
EOF
  chmod 600 "${ENVF}"
fi

cat > "${DESKTOP}" <<EOF
[Desktop Entry]
Type=Application
Name=Overlay Chat
Comment=Always-on-top transparent chat overlay
Exec=${BIN}
Terminal=false
X-GNOME-Autostart-enabled=true
StartupNotify=false
Categories=Utility;
EOF

echo "Autostart installed: ${DESKTOP}"
echo
echo "START: ${BIN}"
echo "END:   ${BIN} --quit"
echo
echo "Set your key in ${ENVF} then log out/in, or run START now."
bash "${ROOT}/scripts/install-hotkey.sh" || true
