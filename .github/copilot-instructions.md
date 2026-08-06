# Copilot Instructions for This Workspace

## Project Context
This workspace targets Arduino 101 (Intel Curie, retired board) development on Windows.
Primary board connection currently appears on COM5.

## Goals
- Keep setup instructions compatible with Arduino IDE 1.8.19 first.
- Prefer stability over modern tooling for this board.
- Provide copy/paste friendly steps for firmware update and sketch upload.

## Technical Constraints
- Intel Curie board package is deprecated.
- Board setup may require proxy configuration on corporate networks.
- Firmware update flow should prioritize IDE 1.8.19.

## Preferred Guidance
When answering requests in this workspace:
1. Assume Arduino/Genuino 101 board.
2. Include board and port checks: Board = Arduino/Genuino 101, Port = COM5.
3. Include Windows driver guidance when board is shown as generic USB Serial Device.
4. Include MASTER RESET timing workaround for upload/firmware issues.
5. Suggest Curie examples (Blink, CurieIMU, CurieBLE) for validation.

## Network/Install Notes
- Boards Manager URL:
  https://downloads.arduino.cc/packages/package_index.json
- If behind proxy, mention host/port configuration in IDE preferences.

## Response Style
- Keep steps concise and ordered.
- Prefer practical troubleshooting before deep theory.
- Ask for checkpoint confirmations after each major setup step.
