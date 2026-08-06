# Central Controller Plan for Haunted House Devices

## Goal
Build a central controller on a Rocky Linux server that can configure, manage, monitor, and manually trigger a mixed fleet of devices:
- Arduino 101 nodes
- ESP32 nodes
- ESP8266 nodes
- UNO-based simple I/O nodes where needed

The controller should provide:
- A web interface for operators
- Device registration and status monitoring
- Trigger configuration and live event logging
- Manual test controls for relays, scenes, and effects
- A simple way to add or replace devices without redesigning the system

## Recommended Architecture
Use a Rocky Linux server as the central application host.

Core components:
- `Mosquitto` for device messaging
- `FastAPI` for the web app and API
- `PostgreSQL` for persistent configuration and logs
- `Nginx` as a reverse proxy and static asset host
- `systemd` or `docker compose` for service management

Why this approach:
- Rocky Linux is stable and suitable for always-on control systems.
- MQTT is a good fit for many small devices reporting state and receiving commands.
- A web server on Linux is much easier to expand than an embedded controller.
- The system can grow from a few boards to dozens without changing the overall pattern.

## High-Level Design
The server is the source of truth for:
- Device identity
- Trigger mappings
- Scene definitions
- Operator actions
- Event history

Devices are responsible for:
- Reporting status and heartbeats
- Listening for commands
- Firing outputs locally
- Sending acknowledgements and errors back to the server

Suggested separation:
- Server handles logic, persistence, and UI
- Devices handle I/O and local actions
- No device should depend on another device being online to keep its own outputs working

## Device Roles
### ESP32
Best all-around network node.
Use for:
- Relay boards
- Sensor boards
- LED effects
- Wireless trigger nodes
- Local display or status devices

### ESP8266
Good low-cost network node.
Use for:
- Simple relay modules
- Basic sensors
- Trigger receivers
- Budget distributed control points

### Arduino 101
Use as a managed endpoint, not the main hub.
Use for:
- RF receiver plus relay action nodes
- Special effect triggers
- Local logic tied to specific radio inputs

### UNO
Use only for simple I/O or legacy devices.
Use for:
- Basic relay control
- Simple serial-connected I/O
- Specialized tasks where no networking is required

If an UNO must be networked, pair it with a small network bridge or keep it attached to a network-capable companion.

## Communication Model
Use MQTT as the primary transport.

Why MQTT:
- Lightweight for microcontrollers
- Easy topic-based routing
- Good publish/subscribe model for many devices
- Natural fit for status, command, and telemetry messages

Suggested topic structure:
- `haunt/<device-id>/status`
- `haunt/<device-id>/heartbeat`
- `haunt/<device-id>/event`
- `haunt/<device-id>/trigger`
- `haunt/<device-id>/config`
- `haunt/<device-id>/ack`
- `haunt/<device-id>/error`

Optional group topics:
- `haunt/group/<group-id>/trigger`
- `haunt/scene/<scene-id>/run`

## Device Lifecycle
### Registration
Each device should have a unique ID.

On startup it should publish:
- Device ID
- Firmware version
- Hardware type
- Capability list
- Current online status

### Heartbeat
Each device publishes a heartbeat at a fixed interval.
The server uses this to determine online/offline state.

### Commands
The server publishes commands to a device topic.
Examples:
- Fire relay 1 for 250 ms
- Toggle relay 3
- Load configuration block
- Enter learning mode

### Acknowledgement
Each device should acknowledge:
- Command received
- Command completed
- Command failed

This is especially useful for haunted-house show control where timing matters.

## Data Model
A PostgreSQL-backed schema is recommended.

Main entities:
- Devices
- Device capabilities
- Groups
- Scenes
- Triggers
- Actions
- Event logs
- Operator sessions

Suggested tables:
- `devices`
- `device_capabilities`
- `groups`
- `scenes`
- `scene_steps`
- `triggers`
- `actions`
- `device_configs`
- `event_log`
- `operator_audit`

Useful fields for devices:
- `id`
- `name`
- `type`
- `model`
- `firmware_version`
- `last_seen_at`
- `status`
- `ip_address`
- `notes`

Useful fields for triggers:
- `id`
- `name`
- `source_type`
- `source_value`
- `target_device_id`
- `target_action`
- `cooldown_ms`
- `enabled`

## Web UI Scope
The web interface should focus on operator workflow, not configuration complexity.

Core screens:
- Dashboard
- Device list
- Device detail
- Trigger editor
- Scene editor
- Live event log
- Manual test panel
- Health and diagnostics
- Settings and network status

Dashboard should show:
- Online/offline counts
- Active alerts
- Recent trigger activity
- Last-seen time for each device
- Fast access to manual overrides

Device detail should show:
- Device metadata
- Capability list
- Current config
- Recent events
- Manual trigger buttons
- Last heartbeat and error state

Trigger editor should support:
- Mapping one source event to one or more outputs
- Delay and cooldown rules
- Toggle actions
- Momentary pulse actions
- Scene membership

## Trigger Logic
The controller should support several trigger types.

### Momentary Pulse
Fire an output for a fixed duration and return it to off.
Useful for:
- Door strikes
- Solenoids
- Light flashes
- Brief relay activations

### Locked Pulse
Fire once, then block repeat triggers for a configured cooldown.
Useful for:
- Effects that should not spam
- Props that need recovery time
- Preventing accidental double-fires

### Toggle
Flip a relay or output on each trigger.
Useful for:
- Persistent lights
- Enabled/disabled states
- Latching effects

### Scene Trigger
Run a multi-step sequence with delays between steps.
Useful for:
- Multi-prop haunted-house scenes
- Lighting and sound coordination
- Coordinated start/stop behavior

## Suggested Operator Workflow
1. Open the web dashboard.
2. Check that all devices are online.
3. Select a device or scene.
4. Assign or verify trigger mappings.
5. Test outputs from the manual panel.
6. Arm the system for show mode.
7. Monitor events and acknowledgements during operation.
8. Review logs after the show.

## API Shape
A REST API is a good starting point.

Suggested endpoints:
- `GET /api/devices`
- `GET /api/devices/{id}`
- `POST /api/devices/{id}/command`
- `POST /api/devices/{id}/config`
- `GET /api/scenes`
- `POST /api/scenes`
- `GET /api/triggers`
- `POST /api/triggers`
- `GET /api/events`
- `GET /api/health`

Optional real-time layer:
- WebSocket or Server-Sent Events for live status updates

## Configuration Strategy
Keep configuration split into two layers:

### Server-Side Configuration
Stored centrally in PostgreSQL.
This includes:
- Device names
- Trigger mappings
- Scene definitions
- Operator settings
- Global rules

### Device-Side Configuration
Stored locally on the device when needed.
This includes:
- Device ID
- Local fallback trigger settings
- Relay timing values
- RF code mappings where relevant

This lets a device continue working if the network is temporarily unavailable.

## Security
Even for a local haunted-house network, basic security matters.

Minimum controls:
- Put the controller on a private LAN or VLAN
- Require login for the web UI
- Protect write actions with authentication
- Use service accounts for devices
- Log operator changes

Optional controls:
- HTTPS with a local certificate
- MQTT authentication
- Role-based access for operators and admins

## Deployment Plan
### Option A: Simple Service Deployment
- Install dependencies directly on Rocky Linux
- Run Mosquitto as a system service
- Run FastAPI with Uvicorn or Gunicorn
- Put Nginx in front of the app
- Use systemd for startup and supervision

### Option B: Container Deployment
- Use Docker or Podman
- Run Mosquitto, API, and database as containers
- Manage the stack with compose files
- Easier to move between servers

For a first version, container deployment is often simpler to reproduce.

## Monitoring
Track:
- Device heartbeat age
- Trigger success/failure
- Command latency
- Relay fire counts
- Error rates
- Offline devices

Useful alerts:
- Device missed heartbeat
- Scene command failed
- MQTT broker unreachable
- Database connection lost
- Too many repeated trigger events

## Development Phases
### Phase 1: Core Infrastructure
- Set up Rocky Linux host
- Install MQTT broker
- Build the API skeleton
- Create the database schema
- Make one test device report status

### Phase 2: Web UI
- Build dashboard
- Build device list and detail pages
- Add event log view
- Add basic auth

### Phase 3: Trigger Control
- Add trigger editor
- Add manual test actions
- Add scene support
- Add cooldown and toggle rules

### Phase 4: Mixed Device Support
- Add ESP32 device templates
- Add ESP8266 templates
- Add Arduino 101 endpoint support
- Add UNO support where practical

### Phase 5: Operations Hardening
- Add audit logging
- Add backups
- Add offline warnings
- Add restore and recovery steps

## Suggested Repo Layout
```text
central-controller/
  app/
    api/
    auth/
    devices/
    scenes/
    triggers/
    templates/
    ui/
  infra/
    nginx/
    mosquitto/
    systemd/
    docker/
  migrations/
  tests/
  docs/
  scripts/
```

## Practical Recommendation
For this project, the strongest approach is:
- Rocky Linux server as the hub
- MQTT for device messaging
- FastAPI for the application layer
- PostgreSQL for state and logs
- Web UI for configuration and monitoring
- Simple device firmware on each board type

This gives you a scalable platform that can manage both the Arduino 101 group and the ESP/UNO fleet without forcing every device to run the same code or networking stack.

## Next Build Step
The next useful artifact is a concrete implementation plan with:
- exact package list for Rocky Linux
- MQTT topic naming rules
- database schema SQL
- first API endpoints
- first web pages
- device message format examples

If you want, that can be the next document or I can scaffold the project structure directly.
