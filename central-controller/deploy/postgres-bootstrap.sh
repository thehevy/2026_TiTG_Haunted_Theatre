#!/usr/bin/env bash
set -euo pipefail

DB_NAME="${1:-haunt}"
DB_USER="${2:-haunt}"
DB_PASS="${3:-}"

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run as root: sudo bash deploy/postgres-bootstrap.sh [db_name] [db_user] [db_password]"
  exit 1
fi

if [[ -z "${DB_PASS}" ]]; then
  DB_PASS="$(tr -dc A-Za-z0-9 < /dev/urandom | head -c 24)"
fi

echo "[1/5] Installing PostgreSQL server..."
dnf install -y postgresql-server postgresql

echo "[2/5] Initializing database cluster if needed..."
if [[ ! -f /var/lib/pgsql/data/PG_VERSION ]]; then
  postgresql-setup --initdb
fi

echo "[3/5] Enabling and starting PostgreSQL..."
systemctl enable --now postgresql

echo "[4/5] Creating role and database..."
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

echo "[5/5] Bootstrap complete."
echo "Database: ${DB_NAME}"
echo "User: ${DB_USER}"
echo "Password: ${DB_PASS}"
echo "Use this DATABASE_URL in /opt/haunt-controller/.env:"
echo "DATABASE_URL=postgresql+psycopg://${DB_USER}:${DB_PASS}@127.0.0.1:5432/${DB_NAME}"
