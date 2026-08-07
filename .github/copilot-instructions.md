# Copilot Instructions for This Workspace

**Created:** 2026-07-30
**Last Updated:** 2026-08-07

## Project Context

This workspace manages a mixed haunted-theatre control stack:

- Arduino 101 firmware nodes (Intel Curie, retired board)
- ESP32 and ESP8266 network nodes
- UNO/Nano class local trigger nodes
- Rocky Linux central web/MQTT controller

Primary board connection frequently appears on COM5 for Arduino 101 testing.

## Goals

- Keep Arduino 101 setup compatible with Arduino IDE 1.8.19 first
- Keep wiring/pin guidance synchronized with sketch constants
- Prefer stable deployment and operational safety over novelty
- Provide copy/paste friendly setup, deployment, and troubleshooting steps

## Technical Constraints

- Intel Curie board package is deprecated
- Board setup may require proxy configuration on corporate networks
- Firmware update flow for Arduino 101 should prioritize IDE 1.8.19
- Some sketches use active LOW relay assumptions; docs must call this out
- ESP pin availability varies by board variant; avoid unsafe default GPIO choices

## Preferred Guidance

When answering requests in this workspace:

1. For Arduino 101 tasks, include board and port checks: Board = Arduino/Genuino 101, Port = COM5 when applicable
2. Include Windows driver guidance when board is shown as generic USB Serial Device
3. Include MASTER RESET timing workaround for upload/firmware issues
4. Keep pinout docs and sketch pin constants in agreement
5. For controller tasks, prefer Rocky Linux friendly commands and service-safe changes
6. For trigger logic, be explicit about active HIGH/LOW behavior and lockout behavior

## Network/Install Notes

- Boards Manager URL: [Arduino package index](https://downloads.arduino.cc/packages/package_index.json)

- If behind proxy, mention host/port configuration in IDE preferences
- For controller deployment, keep MQTT and database credentials in .env, not in committed source

## Response Style

- Keep steps concise and ordered
- Prefer practical troubleshooting before deep theory
- Ask for checkpoint confirmations after each major setup step
- When editing docs, preserve copy/paste command fidelity
