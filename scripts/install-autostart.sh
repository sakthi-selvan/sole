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
# NVIDIA_API_KEY=replace-me
# NVIDIA_API_BASE=https://integrate.api.nvidia.com/v1
# NVIDIA_MODEL=deepseek-ai/deepseek-v4-flash-0731
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
