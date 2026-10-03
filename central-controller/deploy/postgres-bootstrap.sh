#!/usr/bin/env bash
set -Eeuo pipefail

log() {
  echo "[$(date '+%H:%M:%S')] $*"
}

fail() {
  local message="${1:-Database bootstrap failed.}"
  local guidance="${2:-Review the error above and rerun the script with the required values.}"
  echo "ERROR: ${message}" >&2
  echo "Recommended action: ${guidance}" >&2
  exit 1
}

handle_err() {
  local line_no="$1"
  local command="$2"
  trap - ERR
  fail "Command failed at line ${line_no}: ${command}" "Check the PostgreSQL install logs, confirm you are root, and rerun: sudo bash deploy/postgres-bootstrap.sh <db_name> <db_user> <db_password>"
}

trap 'handle_err "${LINENO}" "${BASH_COMMAND}"' ERR

DB_NAME="${1:-haunt}"
DB_USER="${2:-haunt}"
DB_PASS="${3:-}"

if [[ "${EUID}" -ne 0 ]]; then
  fail "This script must be run as root." "Run: sudo bash deploy/postgres-bootstrap.sh [db_name] [db_user] [db_password]"
fi

if [[ -z "${DB_PASS}" ]]; then
  fail "Database password was not supplied on the command line." "Run: sudo bash deploy/postgres-bootstrap.sh ${DB_NAME} ${DB_USER} 'yourStrongPassword' and then update DATABASE_URL to match the same password."
fi

log "[1/5] Installing PostgreSQL server..."
dnf install -y postgresql-server postgresql

log "[2/5] Initializing database cluster if needed..."
if [[ ! -f /var/lib/pgsql/data/PG_VERSION ]]; then
  postgresql-setup --initdb
fi

log "[3/5] Enabling and starting PostgreSQL..."
systemctl enable --now postgresql

log "[4/5] Creating role and database..."
sudo -u postgres psql <<SQL
DO
\$\$
BEGIN
  IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = '${DB_USER}') THEN
    CREATE ROLE ${DB_USER} LOGIN PASSWORD '${DB_PASS}';
  ELSE
    ALTER ROLE ${DB_USER} WITH LOGIN PASSWORD '${DB_PASS}';
  END IF;
END
\$\$;
SQL

if ! sudo -u postgres psql -tAc "SELECT 1 FROM pg_database WHERE datname='${DB_NAME}'" | grep -q 1; then
  sudo -u postgres createdb -O "${DB_USER}" "${DB_NAME}"
fi

sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE ${DB_NAME} TO ${DB_USER};"

if grep -Eq '^(host|local)\s+.*\s+(ident|peer)\b' /var/lib/pgsql/data/pg_hba.conf; then
  echo "WARNING: pg_hba.conf still allows ident/peer auth for local access."
  echo "Recommended action: update the relevant line to something like: host all all 127.0.0.1/32 scram-sha-256"
  echo "Then reload PostgreSQL with: systemctl reload postgresql"
fi

if ! PGPASSWORD="${DB_PASS}" psql -h 127.0.0.1 -U "${DB_USER}" -d "${DB_NAME}" -c "SELECT 1;" >/dev/null 2>&1; then
  fail "Database authentication failed for ${DB_USER}@${DB_NAME}." "Check pg_hba.conf, enable password auth, and rerun: sudo bash deploy/postgres-bootstrap.sh ${DB_NAME} ${DB_USER} 'yourStrongPassword'"
fi

log "[5/5] Bootstrap complete."
log "Database: ${DB_NAME}"
log "User: ${DB_USER}"
log "Password: ${DB_PASS}"
log "Use this DATABASE_URL in /opt/haunt-controller/.env:"
log "DATABASE_URL=postgresql+psycopg://${DB_USER}:${DB_PASS}@127.0.0.1:5432/${DB_NAME}"
