#!/usr/bin/env bash
set -euo pipefail

APP_USER="haunt"
APP_GROUP="haunt"
APP_DIR="/opt/haunt-controller"
SERVICE_NAME="haunt-controller"

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run as root: sudo bash deploy/rocky-install.sh"
  exit 1
fi

echo "[1/8] Installing OS packages..."
dnf install -y python3 python3-pip python3-virtualenv mosquitto nginx policycoreutils-python-utils rsync

echo "[2/8] Enabling broker and web server..."
systemctl enable --now mosquitto
systemctl enable --now nginx

echo "[3/8] Creating app user/group..."
if ! getent group "${APP_GROUP}" >/dev/null; then
  groupadd --system "${APP_GROUP}"
fi
if ! id -u "${APP_USER}" >/dev/null 2>&1; then
  useradd --system --gid "${APP_GROUP}" --create-home --home-dir "/home/${APP_USER}" --shell /sbin/nologin "${APP_USER}"
fi

echo "[4/8] Preparing app directory..."
mkdir -p "${APP_DIR}"

# Copy current repository central-controller contents into target app dir.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

rsync -a --delete \
  --exclude ".venv" \
  --exclude "__pycache__" \
  --exclude "*.pyc" \
  "${SRC_DIR}/" "${APP_DIR}/"

chown -R "${APP_USER}:${APP_GROUP}" "${APP_DIR}"

echo "[5/8] Creating Python virtual environment..."
if [[ ! -d "${APP_DIR}/.venv" ]]; then
  sudo -u "${APP_USER}" python3 -m venv "${APP_DIR}/.venv"
fi
sudo -u "${APP_USER}" "${APP_DIR}/.venv/bin/pip" install --upgrade pip
sudo -u "${APP_USER}" "${APP_DIR}/.venv/bin/pip" install -r "${APP_DIR}/requirements.txt"

echo "[6/8] Creating runtime env file if missing..."
if [[ ! -f "${APP_DIR}/.env" ]]; then
  cp "${APP_DIR}/.env.example" "${APP_DIR}/.env"
  chown "${APP_USER}:${APP_GROUP}" "${APP_DIR}/.env"
  chmod 640 "${APP_DIR}/.env"
  echo "Created ${APP_DIR}/.env. Edit MQTT values before production use."
fi

echo "[7/8] Installing systemd service..."
install -m 644 "${APP_DIR}/systemd/haunt-controller.service" "/etc/systemd/system/${SERVICE_NAME}.service"
sed -i "s|/opt/haunt-controller|${APP_DIR}|g" "/etc/systemd/system/${SERVICE_NAME}.service"
systemctl daemon-reload
systemctl enable --now "${SERVICE_NAME}"

echo "[8/8] Installing nginx site config..."
install -m 644 "${APP_DIR}/nginx/haunt-controller.conf" /etc/nginx/conf.d/haunt-controller.conf
nginx -t
systemctl reload nginx

echo "\nInstall complete. Verify services:"
echo "  systemctl status mosquitto"
echo "  systemctl status ${SERVICE_NAME}"
echo "  systemctl status nginx"
echo "Then open: http://<server-ip>/"
