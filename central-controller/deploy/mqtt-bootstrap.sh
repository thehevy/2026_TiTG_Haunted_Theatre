#!/usr/bin/env bash
set -euo pipefail

MQTT_USER="${1:-haunt-device}"
MQTT_PASS="${2:-}"

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run as root: sudo bash deploy/mqtt-bootstrap.sh [username] [password]"
  exit 1
fi

if [[ -z "${MQTT_PASS}" ]]; then
  MQTT_PASS="$(tr -dc A-Za-z0-9 < /dev/urandom | head -c 24)"
fi

PASSWD_FILE="/etc/mosquitto/passwd"
CONF_FILE="/etc/mosquitto/conf.d/auth.conf"

echo "Creating/updating MQTT user '${MQTT_USER}'..."
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

echo "MQTT credentials configured."
echo "Username: ${MQTT_USER}"
echo "Password: ${MQTT_PASS}"
echo "Update central-controller/.env with MQTT_USERNAME and MQTT_PASSWORD."