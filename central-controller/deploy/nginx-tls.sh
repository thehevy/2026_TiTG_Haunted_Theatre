#!/usr/bin/env bash
# Optional: serve the controller over HTTPS with a self-signed certificate (browsers warn once; accept it per device).
set -Eeuo pipefail

log() {
  echo "[$(date '+%H:%M:%S')] $*"
}

fail() {
  echo "ERROR: ${1}" >&2
  echo "Recommended action: ${2}" >&2
  exit 1
}

trap 'fail "Command failed at line ${LINENO}: ${BASH_COMMAND}" "Check the output above, then rerun: sudo bash deploy/nginx-tls.sh [hostname]"' ERR

if [[ "${EUID}" -ne 0 ]]; then
  fail "This script must be run as root." "Run: sudo bash deploy/nginx-tls.sh [hostname]"
fi

HOST_NAME="${1:-$(hostname -f)}"
if [[ ! "${HOST_NAME}" =~ ^[A-Za-z0-9.-]{1,253}$ ]]; then
  fail "Invalid hostname '${HOST_NAME}'." "Pass the name or IP the browsers use, e.g. sudo bash deploy/nginx-tls.sh haunt-controller.local"
fi

CERT_DIR="/etc/pki/haunt-controller"
CERT="${CERT_DIR}/haunt-controller.crt"
KEY="${CERT_DIR}/haunt-controller.key"
SITE="/etc/nginx/conf.d/haunt-controller.conf"

log "[1/3] Creating a self-signed certificate for ${HOST_NAME} (10 years)..."
install -d -m 755 "${CERT_DIR}"
if [[ "${HOST_NAME}" =~ ^[0-9.]+$ ]]; then
  SAN="IP:${HOST_NAME}"
else
  SAN="DNS:${HOST_NAME}"
fi
SERVER_IP="$(hostname -I 2>/dev/null | awk '{print $1}')"
if [[ -n "${SERVER_IP}" && "${SERVER_IP}" != "${HOST_NAME}" ]]; then
  SAN="${SAN},IP:${SERVER_IP}"
fi
openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
  -keyout "${KEY}" -out "${CERT}" \
  -subj "/CN=${HOST_NAME}" -addext "subjectAltName=${SAN}"
chmod 600 "${KEY}"

log "[2/3] Writing the Nginx site (HTTP redirects to HTTPS)..."
cat > "${SITE}" << EOF
server {
    listen 80;
    listen [::]:80;
    server_name _;
    return 301 https://\$host\$request_uri;
}

server {
    listen 443 ssl;
    listen [::]:443 ssl;
    server_name _;

    ssl_certificate ${CERT};
    ssl_certificate_key ${KEY};
    ssl_protocols TLSv1.2 TLSv1.3;

    client_max_body_size 2m;
    add_header Strict-Transport-Security "max-age=31536000" always;

    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
        proxy_read_timeout 120s;
    }
}
EOF

log "[3/3] Checking and reloading Nginx..."
nginx -t
systemctl reload nginx
if command -v firewall-cmd >/dev/null 2>&1 && systemctl is-active --quiet firewalld; then
  firewall-cmd --permanent --add-service=https >/dev/null
  firewall-cmd --reload >/dev/null
fi

log "Done. Open https://${HOST_NAME}/ and accept the certificate warning once per browser."
log "Certificate SHA-256 fingerprint (compare it in the browser's certificate details):"
openssl x509 -in "${CERT}" -noout -fingerprint -sha256
log "To go back to plain HTTP: sudo install -m 644 /opt/haunt-controller/nginx/haunt-controller.conf ${SITE} && sudo systemctl reload nginx"
