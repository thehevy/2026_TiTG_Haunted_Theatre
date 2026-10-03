#!/usr/bin/env bash
set -Eeuo pipefail

log() {
  echo "[$(date '+%H:%M:%S')] $*"
}

fail() {
  local message="${1:-Rocky installation failed.}"
  local guidance="${2:-Review the error above and rerun the installer after fixing the prerequisite.}"
  echo "ERROR: ${message}" >&2
  echo "Recommended action: ${guidance}" >&2
  exit 1
}

handle_err() {
  local line_no="$1"
  local command="$2"
  trap - ERR
  fail "Command failed at line ${line_no}: ${command}" "Check the output above, confirm all Rocky prereqs are installed, and rerun: sudo bash deploy/rocky-install.sh"
}

trap 'handle_err "${LINENO}" "${BASH_COMMAND}"' ERR

APP_USER="haunt"
APP_GROUP="haunt"
APP_DIR="/opt/haunt-controller"
SERVICE_NAME="haunt-controller"

if [[ "${EUID}" -ne 0 ]]; then
  fail "This script must be run as root." "Run: sudo bash deploy/rocky-install.sh"
fi

log "[1/9] Enabling Rocky prerequisites and package dependencies..."
dnf install -y epel-release

dnf install -y \
  python3 \
  python3-pip \
  python3-virtualenv \
  git \
  curl \
  NetworkManager \
  NetworkManager-wifi \
  NetworkManager-config-server \
  mosquitto \
  nginx \
  policycoreutils-python-utils \
  rsync

log "[2/9] Enabling broker and web server..."
systemctl enable --now NetworkManager
systemctl enable --now mosquitto
systemctl enable --now nginx

log "[3/9] Creating app user/group..."
if ! getent group "${APP_GROUP}" >/dev/null; then
  groupadd --system "${APP_GROUP}"
fi
if ! id -u "${APP_USER}" >/dev/null 2>&1; then
  useradd --system --gid "${APP_GROUP}" --create-home --home-dir "/home/${APP_USER}" --shell /sbin/nologin "${APP_USER}"
fi

log "[4/9] Preparing app directory..."
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

log "[5/9] Creating Python virtual environment..."
if [[ ! -d "${APP_DIR}/.venv" ]]; then
  sudo -u "${APP_USER}" python3 -m venv "${APP_DIR}/.venv"
fi
sudo -u "${APP_USER}" "${APP_DIR}/.venv/bin/pip" install --upgrade pip
sudo -u "${APP_USER}" "${APP_DIR}/.venv/bin/pip" install -r "${APP_DIR}/requirements.txt"

log "[6/9] Creating runtime env file if missing..."
if [[ ! -f "${APP_DIR}/.env" ]]; then
  cp "${APP_DIR}/.env.example" "${APP_DIR}/.env"
  chown "${APP_USER}:${APP_GROUP}" "${APP_DIR}/.env"
  chmod 640 "${APP_DIR}/.env"
  log "Created ${APP_DIR}/.env. Edit MQTT values before production use."
fi

if ! grep -Eq '^DATABASE_URL=' "${APP_DIR}/.env"; then
  fail "The application environment file is missing DATABASE_URL." "Copy .env.example to .env, set DATABASE_URL, and rerun the installer."
fi

log "[7/9] Installing systemd service..."
install -m 644 "${APP_DIR}/systemd/haunt-controller.service" "/etc/systemd/system/${SERVICE_NAME}.service"
sed -i "s|/opt/haunt-controller|${APP_DIR}|g" "/etc/systemd/system/${SERVICE_NAME}.service"
systemctl daemon-reload
systemctl enable --now "${SERVICE_NAME}"

log "[8/9] Installing nginx site config..."
install -m 644 "${APP_DIR}/nginx/haunt-controller.conf" /etc/nginx/conf.d/haunt-controller.conf
nginx -t
systemctl reload nginx

log "[9/9] Validating the installation..."
if ! curl -fsS "http://127.0.0.1:8080/api/health" >/tmp/haunt-controller-health.txt 2>/dev/null; then
  echo "Health check failed. Review the app log and PostgreSQL auth state." >&2
  echo "Recommended action: ensure PostgreSQL is bootstrapped with a password-based auth rule, then rerun: sudo bash deploy/postgres-bootstrap.sh haunt haunt 'yourStrongPassword'" >&2
  journalctl -u "${SERVICE_NAME}" -n 50 --no-pager || true
  fail "Application health check did not pass." "Run the PostgreSQL bootstrap first, confirm DATABASE_URL is correct, and then retry the install."
fi

log "Install complete. Verify services:"
log "  systemctl status NetworkManager"
log "  systemctl status mosquitto"
log "  systemctl status ${SERVICE_NAME}"
log "  systemctl status nginx"
log "  curl http://127.0.0.1:8080/api/health"
log "Then open: http://<server-ip>/"
