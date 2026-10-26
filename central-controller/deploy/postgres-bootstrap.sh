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
  fail "Command failed at line ${line_no}: ${command}" "Check the PostgreSQL install logs, confirm you are root, and rerun: sudo bash deploy/postgres-bootstrap.sh <db_name> <db_user>"
}

trap 'handle_err "${LINENO}" "${BASH_COMMAND}"' ERR

DB_NAME="${1:-haunt}"
DB_USER="${2:-haunt}"

if [[ "${EUID}" -ne 0 ]]; then
  fail "This script must be run as root." "Run: sudo bash deploy/postgres-bootstrap.sh [db_name] [db_user]"
fi

if [[ $# -gt 2 ]]; then
  fail "Passwords are no longer accepted on the command line (they end up in shell history)." "Run: sudo bash deploy/postgres-bootstrap.sh ${DB_NAME} ${DB_USER} and type the password at the prompt."
fi

# Names go into SQL as identifiers, so only allow plain lowercase names.
for name in "${DB_NAME}" "${DB_USER}"; do
  if [[ ! "${name}" =~ ^[a-z_][a-z0-9_]{0,62}$ ]]; then
    fail "Invalid database or user name '${name}'." "Use lowercase letters, digits, and '_' (start with a letter)."
  fi
done

read -rsp "Password for database user '${DB_USER}': " DB_PASS; echo
read -rsp "Repeat the password: " DB_PASS2; echo
if [[ -z "${DB_PASS}" || "${DB_PASS}" != "${DB_PASS2}" ]]; then
  fail "The passwords were empty or did not match." "Rerun the script and type the same password twice."
fi
unset DB_PASS2
sq="'"
DB_PASS_SQL="${DB_PASS//${sq}/${sq}${sq}}"

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
    CREATE ROLE ${DB_USER} LOGIN PASSWORD '${DB_PASS_SQL}';
  ELSE
    ALTER ROLE ${DB_USER} WITH LOGIN PASSWORD '${DB_PASS_SQL}';
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
  fail "Database authentication failed for ${DB_USER}@${DB_NAME}." "Check pg_hba.conf, enable password auth, and rerun: sudo bash deploy/postgres-bootstrap.sh ${DB_NAME} ${DB_USER}"
fi

log "[5/5] Bootstrap complete."
log "Database: ${DB_NAME}"
log "User: ${DB_USER}"
log "Set this in /opt/haunt-controller/.env, with your password in place of PASSWORD (URL-encode characters like @ : / %):"
log "DATABASE_URL=postgresql+psycopg://${DB_USER}:PASSWORD@127.0.0.1:5432/${DB_NAME}"
