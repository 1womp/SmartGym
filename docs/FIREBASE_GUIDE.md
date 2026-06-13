# SmartGym Firebase Guide

Language: English | [Espanol](FIREBASE_GUIDE.es.md)

SmartGym uses Firebase Realtime Database as the backend for user profiles, calibration data, session history, summaries, dashboard charts, and device heartbeat.

## Safety Warning

Do not import a full JSON file at the Firebase root unless you intentionally want to replace the entire database.

Do not commit database secrets, service account files, private auth tokens, Wi-Fi passwords, or local `.env` files. Public Firebase web config should still be restricted with proper Firebase rules and domain/app restrictions.

Safer import pattern:

- Import user JSON at `usersByRfid`.
- Import calibration JSON at `calibrations`.
- Import machine config JSON at `machineConfigs`.
- Import session data at the matching `athleteWeeklySessions/{uid}/{weekKey}` node.

## Main RTDB Nodes

### usersByRfid

```text
usersByRfid/{uid}
```

Stores RFID-linked user profile data such as display name, age, height, weight, gender, and preferred goal.

Profile reads happen during RFID user sync. Profile writes should only happen for explicit profile edits or intentional compatibility fields.

### calibrations

```text
calibrations/{uid}/{machineTypeId}
```

Stores user-machine calibration data:

- Has calibration flag.
- Suggested/recommended load.
- User ROM bottom/top/range.
- Confidence.
- Action.
- Reason.
- Goal-specific recommendations.
- Motion target snapshot.
- Calibration set snapshots.
- Next recommended weight fields.

### machineConfigs

```text
machineConfigs/{machineId}
```

Stores cloud machine configuration where available. The firmware also contains an embedded machine catalog so it can boot offline.

### athleteWeeklySessions

```text
athleteWeeklySessions/{uid}/{weekKey}
```

This is the main workout history node for dashboard analysis.

Typical structure:

```text
athleteWeeklySessions/{uid}/{weekKey}/days/{dayKey}/sessions/{sessionId}
athleteWeeklySessions/{uid}/{weekKey}/days/{dayKey}/daySummary
athleteWeeklySessions/{uid}/{weekKey}/days/{dayKey}/timeline/{timelineKey}
athleteWeeklySessions/{uid}/{weekKey}/weekSummary
```

### devices

```text
devices/{deviceId}
```

Stores heartbeat and device identity/status data.

## Session Root

The session root contains the main record for a completed workout:

- Session ID.
- User UID.
- Machine ID/type.
- Goal.
- Started/ended time.
- Duration.
- Selected/pin load.
- Recommended load.
- Set/repetition totals.
- Valid/invalid rep counts.
- ROM and velocity metrics.
- Quality score/tier.
- Links to heavy details.

## daySummary

Daily aggregate summary for fast dashboard reads. It avoids requiring the dashboard to scan every session for common day-level metrics.

## weekSummary

Weekly aggregate summary for the active user/week. It includes summary counters and recent training signal information.

## timeline

Timeline entries are lightweight ordered cards for the dashboard. The timeline key also acts as an idempotency marker so retries do not double-count summaries.

## representativeReps

Stores selected representative reps, such as:

- Best ROM rep.
- Best velocity rep.
- First valid rep.
- Last valid rep.

## setDetails

Stores per-set summary information:

- Set number.
- Target reps.
- Valid reps.
- Invalid reps.
- Start/end selected load.
- Whether weight changed during set.
- ROM/velocity summaries.

## repSets

Stores deeper per-rep data by set:

```text
repSets/set1/reps/0
repSets/set1/reps/1
```

The firmware may upload these details in smaller PATCH payloads, but the final Firebase shape remains unchanged.

## Upload Phases

1. Root session/core data.
2. Day metadata.
3. Timeline.
4. Day summary.
5. Week summary.
6. Representative reps.
7. Set details.
8. Rep sets.

## Memory-Safe Upload

The firmware uses NVS queueing and memory-aware payload splitting. Under constrained heap, repSets are split into smaller patches to avoid Firebase/TLS blocking.

## Dashboard Compatibility

The dashboard currently reads:

- `usersByRfid/{rfid}`
- `athleteWeeklySessions/{rfid}`
- `athleteWeeklySessions/{rfid}/{week}/days`
- `recommendedRoutineProgress/{rfid}/{selectedDate}`

Do not rename the firmware paths without updating the dashboard.
