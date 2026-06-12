# SmartGym UI Handoff (SquareLine)

Hey! You have full design freedom.  
Don’t worry about matching current LVGL layout, I’ll adapt code to your IDs.

## What this app does

Gym machine UI with:
- live coaching while training
- rest countdown between sets
- session summary at the end

## Screens we need

1. Idle
2. Main Training
3. Rest Overlay (modal on top of Main)
4. Summary
5. Calibration Gate
6. Profile
7. Debug/Service (can be basic)

Important: only one start/finish control in Main (`START` / `FINISH` toggle).

## Data you can use on UI (descriptive)

### Live
- **Selected weight (kg)**: current load chosen for the set.
- **Suggested load seed (kg)**: baseline recommended weight before live adjustments.
- **Weight factor**: multiplier used to scale suggested load for user/goal context.
- **Current set / target sets**: progress through the workout plan (example `2 / 4`).
- **Reps in set / target reps**: progress inside the active set (example `7 / 10`).
- **ROM %**: current range of motion depth as a percent (0-100).
- **User ROM bounds**: personalized range limits (`userBottomPct` to `userTopPct`).
- **Velocity (%/s)**: movement speed normalized as percent per second.
- **Rep phase**: live phase label (`LIFT`, `LOWER`, `HOLD`, `REST`).
- **Quality state**: fast coaching state (`READY`, `SLOW`, `SPEED OK`, `TOO FAST`, etc.).
- **Session timer**: elapsed workout time in the active session.
- **Rest countdown**: seconds remaining before the next set.
- **Rest target (`restSeconds`)**: planned rest time for the current exercise/goal.
- **Rep timing (ms)**:
  - `riseMs`: concentric/lift duration
  - `lowerMs`: eccentric/lower duration
  - `topPauseMs`: pause near top position
  - `bottomPauseMs`: pause near bottom position

### Motion guide (important)
- **Ideal motion curve**: target path for a good rep over time.
- **Live dot position**: athlete’s current movement point plotted on that graph.
- **Live halo**: visual emphasis around the live dot so it’s easy to follow quickly.
- **On-path state**: whether live movement is close enough to the ideal path.
- **ROM target band**: highlighted target ROM zone (single fixed chart mode, no expanded graph).

### Session end / summary
- **Quality score + tier**: overall session quality (0-100) and readable tier label.
- **Sets completed**: completed sets vs planned sets.
- **Valid / invalid reps + valid rate**: accepted reps, rejected reps, and success percentage.
- **Average ROM %**: average ROM on valid reps.
- **Average velocity (%/s)**: average rep speed on valid reps.
- **Volume load (kg)**: total effective load (`selected weight * valid reps`).
- **Duration**: total session time (`mm:ss` or similar).
- **Sync/storage status**: whether saved locally, queued, or synced.

### Extra useful values you can show (already available)
- **Best ROM %**: best rep ROM reached in the session.
- **Average eccentric velocity (%/s)**: lowering-phase speed quality signal.
- **Fast eccentric warnings**: count of reps lowered too quickly.
- **Total rest time**: total rest accumulated in session.
- **Average rest per set**: pacing indicator.
- **Rep quality trend**: first sets vs last sets quality change.
- **Fatigue ROM drop**: ROM loss from early to late sets.
- **Fatigue velocity drop**: speed loss from early to late sets.
- **ROM compliance rate**: percent of valid reps that met user ROM target.
- **Ideal ROM hit rate**: percent of valid reps that met machine ideal ROM target.
- **Invalid reason breakdown**:
  - short ROM
  - too fast
  - top not reached
  - no concentric phase

### Data keys to include in UI mapping (important)
- `restSeconds`
- `riseMs`
- `lowerMs`
- `topPauseMs`
- `bottomPauseMs`
- `suggestedLoadSeedKg`
- `weightFactor`
- `userBottomPct`
- `userTopPct`

### Extra coaching metrics available
- **ROM compliance rate**: percent of valid reps that reached user ROM target.
- **Ideal ROM hit rate**: percent of valid reps that reached machine ideal ROM target.
- **Fatigue ROM drop**: how much ROM decreased from early sets to late sets.
- **Fatigue velocity drop**: how much speed decreased from early sets to late sets.
- **Invalid reason counts**: rejection reason totals (`short ROM`, `too fast`, `top not reached`, etc.).

### Update rhythm (so layout expectations are clear)
- Live values (ROM, velocity, phase, quality, timers, rep counters): update continuously during training.
- Rest values: update each second during rest countdown.
- Summary values: update once at session finish, then remain stable on summary screen.

## Profile screen (show + edit)

### Show
- Card UID
- Name
- Age
- Weight (kg)
- Height (cm)
- Goal
- Gender

### Editable
- Name (text input + keyboard)
- Age (`- / +`)
- Weight (`- / +`, 0.5 kg step)
- Height (`- / +`, 1 cm step)
- Goal (prev/next)
- Gender (prev/next)

### Save/Cancel behavior
- `Save`: keep changes and return
- `Cancel`: discard changes and return

### Validation ranges
- Age: 12 to 90
- Weight: 35.0 to 220.0 kg
- Height: 120 to 230 cm

### Profile UX note
- Goal and Gender can be dropdown menus (preferred).

## Debug/Service screen requirements

Keep it practical and fast to use. Styling can be simple.

### Must show
- Debug status text block
- Hardware status text block

### Must include controls
- Machine selector (dropdown)
- ROM injector slider
- Auto rep button
  - short press: one cycle
  - long press/hold: continuous
  - release: stop continuous mode
- Motion reset button
- Jump ROM to top button
- Jump ROM to bottom button
- Cloud sync button
- Sensor mode toggle button
- Log level button
- Encoder buttons:
  - set 0cm
  - set 100cm
  - apply
  - reset
- Quick user buttons:
  - user 1
  - user 2
  - new user
  - anonymous
- Close button

### Debug screen intent
- quick hardware checks
- simulation for demos/testing
- calibration and cloud verification
- user/session test shortcuts

## What to send me back

1. `.slp` project
2. exported LVGL files
3. fonts/images/icons used
4. small mapping table:

| screen | widget_id | meaning |
|---|---|---|
| Main | btn_start_finish | start_or_finish |
| Main | lbl_velocity | live velocity |
| Summary | lbl_summary_main | summary metrics block |

That’s it. I’ll handle the integration.

## Quick event list (so wiring is easy)

Please include widgets for these actions:

- `start_or_finish`
- `weight_minus_large` (`-5`)
- `weight_minus` (`-2.5`)
- `weight_plus` (`+2.5`)
- `weight_plus_large` (`+5`)
- `start_calibration`
- `rest_skip`
- `open_profile`
- `open_service`
- `profile_save`
- `profile_cancel`
- `calibration_gate_calibrate`
- `calibration_gate_skip`

## When each screen appears

- `Idle`: no active workout / waiting for user
- `Main Training`: active training flow
- `Rest Overlay`: between sets during rest countdown
- `Summary`: right after finishing session
- `Calibration Gate`: when user needs calibration decision
- `Profile`: when user edits profile
- `Debug/Service`: service/testing tools

## Fill-this template (copy/paste)

| screen | widget_id | type | meaning |
|---|---|---|---|
| Main |  | button | start_or_finish |
| Main |  | button | weight_minus_large |
| Main |  | button | weight_minus |
| Main |  | button | weight_plus |
| Main |  | button | weight_plus_large |
| Main |  | label | live_rom |
| Main |  | label | live_velocity |
| Main |  | chart | ideal_curve + live_dot |
| Rest Overlay |  | button | rest_skip |
| Summary |  | label | summary_main_metrics |
| Summary |  | label | summary_detail |
| Calibration Gate |  | button | calibration_gate_calibrate |
| Calibration Gate |  | button | calibration_gate_skip |
| Profile |  | dropdown | goal |
| Profile |  | dropdown | gender |
| Profile |  | button | profile_save |
| Profile |  | button | profile_cancel |
