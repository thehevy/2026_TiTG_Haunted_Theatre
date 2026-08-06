#!/usr/bin/env bash
set -euo pipefail

ALLOW_MQTT_PORT="false"
if [[ "${1:-}" == "--allow-mqtt" ]]; then
  ALLOW_MQTT_PORT="true"
fi

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run as root: sudo bash deploy/rocky-harden.sh [--allow-mqtt]"
  exit 1
fi

echo "[1/5] Installing hardening tools..."
dnf install -y firewalld fail2ban policycoreutils-python-utils

echo "[2/5] Enabling firewall and fail2ban..."
systemctl enable --now firewalld
systemctl enable --now fail2ban

echo "[3/5] Applying firewall rules..."
firewall-cmd --permanent --add-service=ssh
firewall-cmd --permanent --add-service=http
firewall-cmd --permanent --add-service=https
if [[ "${ALLOW_MQTT_PORT}" == "true" ]]; then
  firewall-cmd --permanent --add-port=1883/tcp
fi
firewall-cmd --reload

echo "[4/5] Setting SELinux boolean for Nginx reverse proxy..."
setsebool -P httpd_can_network_connect 1

echo "[5/5] Hardening complete."
echo "Firewall open ports/services now:" 
firewall-cmd --list-all