# SmartGym User Manual

This manual explains how to use the SmartGym Adaptive Training System as an evaluator, athlete, or demo user.

## 1. Turning On the Device

1. Connect power to the ESP32-S3 display.
2. Wait for the main screen to appear.
3. Allow the device to connect to Wi-Fi, Firebase, RFID, and time services.
4. Do not start a workout until the device reaches the ready state.

## 2. RFID Login

1. Place the RFID card near the reader.
2. The screen shows a loading popup.
3. Keep waiting until the popup closes.

The popup remains visible while the device loads:

- User profile.
- User calibration for the active machine.
- Recommendation for the active user and machine.
- Calibrated ROM mapping.

The user should not press START or CALIBRATE while the loading popup is active.

## 3. Main Screen

The main screen shows the current session state, live motion graph, pin load, recommendation, reps, sets, and action buttons.

Important terms:

- Pin load: the physical weight pin position on the machine.
- Recommended load: the load suggested by SmartGym for the active user.

The pin load belongs to the machine, not the user. It does not automatically change when a different user scans an RFID card. The recommendation is user-specific advice.

## 4. Pin Load vs Recommended Load

The system never physically moves the weight pin. If the screen recommends 25.0 kg but the machine pin is at 20.0 kg, the device shows both values separately:

- Pin load: 20.0 kg
- Recommended: 25.0 kg

If the user wants to follow the recommendation, they must physically move the pin and then update the displayed pin load with the touchscreen buttons.

## 5. Changing Pin Load

Use the four weight buttons:

- `-5`
- `-2.5`
- `+2.5`
- `+5`

These buttons update the displayed machine pin load. They do not change the recommendation.

Pin load can be changed before training or during training. If it changes during training, later reps are recorded with the new load.

## 6. Starting a Workout

1. Scan RFID and wait for loading to finish.
2. Check the pin load.
3. Physically set the machine pin if needed.
4. Adjust the on-screen pin load to match the physical pin.
5. Press START.

The session starts using the displayed pin load exactly as shown.

## 7. Following the Graph

The graph shows motion through the exercise range. The firmware uses ROM, or range of motion, to detect repetitions.

The user-facing cues focus on timing, for example:

- Rise time.
- Lower time.
- Top hold.
- Bottom hold.

The system avoids showing raw technical values such as `%ROM/s` in the main user interface.

## 8. Reps and Sets

The firmware detects reps from the ROM signal. A rep must move through enough range and pass the internal validation rules.

The screen shows:

- Current reps in the set.
- Current set.
- Training state.
- Timer or progress where available.

If a rep is not counted, possible causes include short range of motion, noisy movement, or not reaching the top/bottom thresholds.

## 9. Rest and Finishing

When the target reps for a set are completed, the system marks the set complete. Depending on the target set count, the session may auto-finish or continue to the next set.

The user can also finish manually by pressing FINISH.

## 10. Summary Screen and Sync

After finishing, the summary screen appears. It may show:

- Session complete.
- Pin load used.
- New recommendation.
- Save/upload status.

Typical save states:

- Saving session...
- Uploading core...
- Uploading details...
- Uploading reps...
- Session saved.
- Saving in background.

If sync is still running, the device can continue saving in the background. Do not power off immediately after a workout if the screen still indicates saving.

## 11. Calibration

Calibration should be used when:

- A new user uses the machine.
- ROM looks incorrect.
- Recommendations are missing.
- The system says to calibrate first.
- The exercise setup has changed.

### Calibration Steps

1. Press CALIBRATE.
2. Read the step title and instruction.
3. Move the physical pin to the suggested load if appropriate.
4. Use the calibration screen's four pin-load buttons if the displayed pin does not match the physical pin.
5. Tap LOAD SET.
6. Perform 3 to 5 smooth reps.
7. Follow any next-load instruction.
8. Save the result when complete.

The calibration screen shows:

- Pin load.
- Suggested load.
- Weight adjustment buttons.
- Rep feedback.
- Save/cancel actions.

If the pin load differs from the suggested load, the user may still continue, but the device logs that the user chose a different physical pin.

## 12. Recommendations

Recommendations are generated from calibration and session history. They are advice only. The system does not automatically change the physical pin load.

Recommendation sources may include:

- Calibration.
- Last session.
- Machine default.
- No recommendation.

## 13. Troubleshooting

### RFID Not Detected

- Hold the card closer to the RFID reader.
- Wait for upload/sync to finish.
- Try scanning again.

### User Still Loading

- Wait for the loading popup to close.
- If cloud read fails, the device should fall back safely.

### Rep Not Counted

- Use full range of motion.
- Move smoothly.
- Reach the top and bottom positions.
- Recalibrate if ROM mapping looks wrong.

### Graph Looks Wrong

- Recalibrate ROM.
- Check the motion sensor connection.
- Confirm the exercise machine setup did not change.

### Sync Still Running

- Keep the device powered.
- Wait for "Session saved" if possible.
- The device may continue saving in the background.

### Recommendation Does Not Change Pin Load

This is expected. The pin load is the physical machine state. The user must move the pin and update the display manually.

### Wi-Fi or Firebase Issues

- Check Wi-Fi credentials.
- Check Firebase RTDB URL/auth configuration.
- Confirm internet connectivity.
- Use serial logs for details.
