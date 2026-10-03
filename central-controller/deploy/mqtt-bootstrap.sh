#!/usr/bin/env bash
set -Eeuo pipefail

log() {
  echo "[$(date '+%H:%M:%S')] $*"
}

fail() {
  local message="${1:-MQTT bootstrap failed.}"
  local guidance="${2:-Review the error above and rerun the script with the required values.}"
  echo "ERROR: ${message}" >&2
  echo "Recommended action: ${guidance}" >&2
  exit 1
}

handle_err() {
  local line_no="$1"
  local command="$2"
  trap - ERR
  fail "Command failed at line ${line_no}: ${command}" "Check the Mosquitto setup logs, confirm you are root, and rerun: sudo bash deploy/mqtt-bootstrap.sh <username> <password>"
}

trap 'handle_err "${LINENO}" "${BASH_COMMAND}"' ERR

MQTT_USER="${1:-haunt-device}"
MQTT_PASS="${2:-}"

if [[ "${EUID}" -ne 0 ]]; then
  fail "This script must be run as root." "Run: sudo bash deploy/mqtt-bootstrap.sh [username] [password]"
fi

if [[ -z "${MQTT_PASS}" ]]; then
  fail "MQTT password was not supplied on the command line." "Run: sudo bash deploy/mqtt-bootstrap.sh ${MQTT_USER} 'yourStrongPassword' and then update central-controller/.env with the exact same MQTT password."
fi

PASSWD_FILE="/etc/mosquitto/passwd"
CONF_FILE="/etc/mosquitto/conf.d/auth.conf"

log "Creating/updating MQTT user '${MQTT_USER}'..."
if [[ -f "${PASSWD_FILE}" ]]; then
  mosquitto_passwd -b "${PASSWD_FILE}" "${MQTT_USER}" "${MQTT_PASS}"
else
  mosquitto_passwd -c -b "${PASSWD_FILE}" "${MQTT_USER}" "${MQTT_PASS}"
fi

cat > "${CONF_FILE}" << 'EOF'
allow_anonymous false
password_file /etc/mosquitto/passwd
listener 1883
EOF

systemctl restart mosquitto
systemctl enable mosquitto

log "MQTT credentials configured."
log "Username: ${MQTT_USER}"
log "Password: ${MQTT_PASS}"
log "Update central-controller/.env with MQTT_USERNAME and MQTT_PASSWORD."