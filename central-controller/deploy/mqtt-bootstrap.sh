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
  fail "Command failed at line ${line_no}: ${command}" "Check the Mosquitto setup logs, confirm you are root, and rerun: sudo bash deploy/mqtt-bootstrap.sh <username>"
}

trap 'handle_err "${LINENO}" "${BASH_COMMAND}"' ERR

MQTT_USER="${1:-haunt-device}"

if [[ "${EUID}" -ne 0 ]]; then
  fail "This script must be run as root." "Run: sudo bash deploy/mqtt-bootstrap.sh [username]"
fi

if [[ $# -gt 1 ]]; then
  fail "Passwords are no longer accepted on the command line (they end up in shell history)." "Run: sudo bash deploy/mqtt-bootstrap.sh ${MQTT_USER} and type the password at the prompt."
fi

if [[ ! "${MQTT_USER}" =~ ^[A-Za-z0-9_.-]{1,64}$ ]]; then
  fail "Invalid MQTT username '${MQTT_USER}'." "Use letters, digits, '.', '_' or '-'."
fi

read -rsp "MQTT password for '${MQTT_USER}': " MQTT_PASS; echo
read -rsp "Repeat the password: " MQTT_PASS2; echo
if [[ -z "${MQTT_PASS}" || "${MQTT_PASS}" != "${MQTT_PASS2}" ]]; then
  fail "The passwords were empty or did not match." "Rerun the script and type the same password twice."
fi
unset MQTT_PASS2

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
log "Set the same username/password in /opt/haunt-controller/.env (MQTT_USERNAME, MQTT_PASSWORD) and on each prop's settings page."