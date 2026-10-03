# Central Controller Deployment Retrospective

**Date:** October 2, 2026

**Repository:** [2026_TiTG_Haunted_Theatre Central Controller](https://github.com/thehevy/2026_TiTG_Haunted_Theatre/blob/main/central-controller/README.md)

## Executive Summary

The deployment was largely successful using the provided installation scripts and documentation. The application installation, Mosquitto deployment, Nginx configuration, and systemd service creation completed successfully.

The primary blocker was PostgreSQL authentication. The application failed to start because PostgreSQL was configured to use Ident authentication while the application expected password-based authentication using the credentials specified in `.env`.

A second deployment issue was also encountered: some required packages were not available until the Rocky EPEL repository was enabled. That prerequisite was not covered by the install scripts or the deployment documentation, which caused an avoidable setup delay during first-time installation.

These issues required manual troubleshooting that is not currently documented in the installation guide and could be a common failure mode for first-time deployments on Rocky Linux systems.

---

## Deployment Sequence Executed

### 1. Application Installation

Executed:

```bash
sudo bash deploy/rocky-install.sh
```

Result:

- Python installed
- Mosquitto installed
- Nginx installed
- Application copied to `/opt/haunt-controller`
- Virtual environment created
- Systemd service created
- Nginx reverse proxy configured

Status: ✅ Successful

---

### 2. Environment Configuration

Copied:

```bash
cp .env.example .env
```

Updated:

```env
DATABASE_URL=postgresql+psycopg://haunt:hauntme@127.0.0.1:5432/haunt
SECRET_KEY=<generated secret>
```

Status: ✅ Successful

---

### 3. PostgreSQL Bootstrap

Executed:

```bash
sudo bash deploy/postgres-bootstrap.sh haunt haunt
```

Result:

- PostgreSQL user `haunt` created
- PostgreSQL database `haunt` created
- Credentials configured

Status: ✅ Successful

---

### 4. MQTT Bootstrap

Executed:

```bash
sudo bash deploy/mqtt-bootstrap.sh haunt-device
```

Result:

- MQTT user created
- Broker running successfully

Status: ✅ Successful

---

### 5. Initial Application Failure

Observed:

```text
sqlalchemy.exc.OperationalError
FATAL: Ident authentication failed for user "haunt"
```

The application continuously restarted while `systemctl status haunt-controller` showed `activating (auto-restart)`.

A call to `curl http://127.0.0.1:8080/api/health` failed because the application never fully started.

Status: ❌ Failure

---

## Root Cause Analysis

### PostgreSQL Authentication Mismatch

The application attempted authentication using:

```env
DATABASE_URL=postgresql+psycopg://haunt:hauntme@127.0.0.1:5432/haunt
```

However, PostgreSQL was configured with:

```text
host all all 127.0.0.1/32 ident
```

This caused PostgreSQL to ignore the supplied password and instead require operating-system identity mapping.

Effectively:

```text
Application supplies password
    ↓
PostgreSQL ignores password
    ↓
Ident authentication attempted
    ↓
Authentication fails
```

---

## Resolution

Changed:

```text
host all all 127.0.0.1/32 ident
```

to:

```text
host all all 127.0.0.1/32 scram-sha-256
```

in `pg_hba.conf`, then reloaded PostgreSQL.

Result:

- Database authentication successful
- Application startup successful
- Health endpoint became available

Status: ✅ Resolved

---

## Documentation Improvement Recommendations

### Priority 0: Add Rocky EPEL Prerequisite Check

The installer assumes the standard Rocky package sources are sufficient, but some packages may require the EPEL repository to be enabled before installation.

Add a setup step before installation:

```bash
sudo dnf install -y epel-release
```

Then re-run the installer or the package install step.

Benefit:

- Avoids package-not-found failures during initial setup.
- Makes the Rocky deployment flow more reliable on stock systems.

---

### Priority 1: Add PostgreSQL Verification Step

Current documentation assumes PostgreSQL is working correctly after bootstrap.

Add:

```bash
PGPASSWORD=<password> psql -h 127.0.0.1 -U haunt -d haunt -c "SELECT version();"
```

Expected output should be shown.

Benefit:

- Catches database issues before attempting application startup.
- Reduces troubleshooting time significantly.

---

### Priority 2: Add Health Check Validation Section

After install, document:

```bash
systemctl status haunt-controller
curl http://127.0.0.1:8080/api/health
```

Expected health response example:

```json
{
  "status": "ok"
}
```

Benefit:

- Provides clear success criteria.

---

### Priority 3: Add Service Log Troubleshooting Section

Add:

```bash
journalctl -u haunt-controller -n 100
journalctl -xeu haunt-controller
```

Benefit:

- Directly leads users to the actual startup exception.

---

### Priority 4: Document PostgreSQL Authentication Expectations

README currently demonstrates:

```env
DATABASE_URL=postgresql+psycopg://haunt:password@127.0.0.1:5432/haunt
```

but does not state that PostgreSQL must support password authentication.

Add:

```text
The application requires PostgreSQL host authentication to use password authentication
(scram-sha-256 or md5). Ident/peer authentication is not supported when using
DATABASE_URL credentials.
```

Benefit:

- Prevents the exact issue encountered during deployment.

---

## SSH Session Recovery Findings and Additional Gaps

The recovery session showed that the deployment was missing several system-level prerequisites that were not captured by the original installers or docs.

### Network and Wi-Fi Setup Was Missing

The commands show repeated NetworkManager and Wi-Fi recovery steps:

- `nmtui`
- `nmcli radio wifi on`
- `nmcli device status`
- `nmcli device wifi list`
- `systemctl restart NetworkManager`
- `hostnamectl` to rename the host

This indicates that the documentation and installation flow did not clearly cover the need to:

- install or enable the Wi-Fi networking stack on Rocky,
- check whether the wireless interface is managed by NetworkManager,
- confirm the device is recognized and enabled before app deployment,
- reconfigure the hostname for the target environment.

Recommended improvement:

- Add a Rocky pre-flight section that validates the wireless interface, NetworkManager state, and host name.
- Document the installation of `NetworkManager`, `NetworkManager-wifi`, and `NetworkManager-config-server` when the system is shipped without a complete desktop/network profile.

### EPEL and Dependency Packages Were Not Included in the Install Path

The session includes:

- `dnf install epel-release -y`
- `dnf install NetworkManager-wifi`
- `dnf install NetworkManager`
- `dnf install git`
- `dnf install -y python3 python3-pip`

This demonstrates that the scripted install path was incomplete for a fresh Rocky host. Several packages were not available until the EPEL repository was enabled or a package was installed manually.

Recommended improvement:

- Add `epel-release` to the installation prerequisites in the docs and install scripts.
- Add a dependency validation step to confirm required packages are present before continuing.
- Add `python3-virtualenv`, `git`, and the NetworkManager pieces to the Rocky install checklist.

### The Install Flow Was Not Fully End-to-End Validated

The session shows repeated reinstall and service restart attempts, including:

- `./rocky-install.sh` run multiple times
- `systemctl restart haunt-controller`
- `systemctl restart mosquitto`
- `systemctl restart nginx`
- `curl http://127.0.0.1:8080/api/health`

This suggests the project needed a dedicated post-install validation loop, not just a package install. The installer should stop early if the service cannot bind or the app health check fails.

Recommended improvement:

- Add a final validation block to the installer that checks `systemctl is-active` for the app, MQTT, and Nginx.
- Run the health endpoint automatically and print the relevant log command on failure.

### Configuration Secrets Were Hand-Edited During Recovery

The session includes multiple manual edits to `.env` values, including:

- generating a secret key with `openssl rand -base64 32`
- replacing `change-me` values in the config
- editing `.env` by hand after install

This is workable, but it is error-prone.

Recommended improvement:

- Add a secure `.env` generation section to the docs.
- Document the use of `openssl rand -base64 32` and explicit environment variable editing.
- Keep `.env` out of source control and confirm the `.gitignore` intent is clear.

### Summary

The SSH session revealed that the real deployment path on Rocky Linux required additional operating-system preparation beyond the original app-level automation. The gaps were not in the app code itself; they were in the environment bootstrap, package dependency assumptions, Wi-Fi configuration, and validation workflow.

---

## Script Improvement Recommendations

### Postgres Bootstrap Validation

Enhance `deploy/postgres-bootstrap.sh`.

After creating the user and database, automatically verify access:

```bash
PGPASSWORD="$DB_PASSWORD" psql -h 127.0.0.1 -U "$DB_USER" -d "$DB_NAME" -c "SELECT 1;"
```

Fail the script if authentication does not work.

Benefit:

- Converts a runtime failure into an install-time failure.
- Easier for users to diagnose.

---

### Automatic pg_hba Validation

The script should inspect `pg_hba.conf`.

If authentication is set to `ident` or `peer`, emit:

```text
WARNING:
Application uses password authentication.
Current PostgreSQL configuration uses IDENT.
Update pg_hba.conf to scram-sha-256.
```

or optionally update it automatically.

Benefit:

- Eliminates the most likely deployment blocker.

---

### Installer End-to-End Verification

At the end of `rocky-install.sh`, perform:

```bash
curl http://127.0.0.1:8080/api/health
```

If unsuccessful:

```bash
journalctl -u haunt-controller -n 50
```

Display the error automatically.

Benefit:

- Immediate validation of a successful deployment.

---

## Suggested README Addition

### Verify Installation

```bash
systemctl status haunt-controller
curl http://127.0.0.1:8080/api/health
```

If startup fails:

```bash
journalctl -xeu haunt-controller
```

Verify PostgreSQL:

```bash
PGPASSWORD=<password> psql -h 127.0.0.1 -U haunt -d haunt
```

If authentication fails and `pg_hba.conf` contains:

```text
host all all 127.0.0.1/32 ident
```

change it to:

```text
host all all 127.0.0.1/32 scram-sha-256
```

Reload PostgreSQL and restart the application.

---

## Overall Assessment

**Installation Automation:** 8/10

**Documentation Clarity:** 7/10

**Troubleshooting Guidance:** 4/10

**Operational Readiness:** 8/10

The deployment architecture is solid and the automation handles most of the setup correctly. The single largest improvement would be adding PostgreSQL authentication validation and a post-install health check. That change alone would likely eliminate the majority of first-time deployment issues encountered during installation.
