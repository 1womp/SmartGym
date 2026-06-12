# SmartGym Troubleshooting Guide

## Build Fails

Run:

```powershell
python -m platformio run --environment BOARD_VIEWE_UEDX80480070E_WB_A
```

Check:

- PlatformIO is installed.
- Board environment name is correct.
- Dependencies downloaded correctly.
- No local syntax changes are incomplete.

## COM Port Busy

Symptoms:

- Upload fails with access denied.
- Monitor cannot open serial port.

Fix:

- Close any open serial monitor.
- Close Arduino IDE or other terminal using the port.
- Unplug/replug the ESP32.
- Retry upload.

## Upload Fails

- Confirm USB cable supports data.
- Confirm correct board/port.
- Try holding BOOT if required by the board.
- Lower upload speed if needed.

## Touch Not Responding

- Confirm LVGL/display initialization succeeded.
- Check serial boot logs.
- Reboot the device.

## RFID Not Detected

- Confirm MFRC522 wiring.
- Check RFID SPI pins.
- Hold card close to the reader.
- Avoid scanning during active upload if firmware is blocking RFID for safety.

## Wi-Fi Not Connected

- Check configured SSID/password build flags.
- Confirm network is 2.4 GHz.
- Check serial Wi-Fi logs.

## Firebase Read/Write Fails

- Confirm database URL and token/config.
- Confirm Wi-Fi is connected.
- Check Firebase RTDB rules.
- Check serial tags `[CloudWrite]`, `[SYNC]`, and `[USER_SYNC]`.

## Sync Queue Stuck

Look for:

- `[SYNC] queue upload delayed`
- `[SYNC] upload memory mode=critical`
- NVS queue count logs.

Keep the device powered until it drains the queue.

## NVS NOT_ENOUGH_SPACE

Cause:

- Too many queued writes or payloads.

Mitigation:

- Firmware uses queue capacity checks and chunking.
- Avoid repeatedly skipping summary during active upload if testing older builds.
- Let the queue drain.

## repSets Upload Delayed by Heap Guard

Cause:

- Firebase/TLS memory pressure.
- Large repSet PATCH payload.

Expected behavior:

- Firmware should split repSets into smaller chunks under constrained memory.
- Upload should eventually reach `webapp upload phase=complete`.

## Graph Clipping or Wrong ROM

- Recalibrate ROM.
- Check sensor wiring.
- Confirm sensor direction/inversion.
- Check whether user calibration was loaded.

## Rep Not Counted

Possible causes:

- ROM too short.
- Top/bottom not reached.
- Motion too fast/noisy.
- Calibration mismatch.

Try smooth reps through full range.

## Calibration Cancel Freeze

If calibration does not return safely:

- Check serial logs around `[CAL]`, `[CAL_UI]`, and UI destroy paths.
- Reboot device if needed.
- Verify no stale LVGL pointer accesses were introduced.

## Dashboard Missing Data

Check:

- Correct RFID UID selected.
- Firebase path exists under `athleteWeeklySessions/{uid}`.
- Session upload reached `phase=complete`.
- Browser console for Firebase permission errors.
- Dashboard Firebase config matches firmware database.
