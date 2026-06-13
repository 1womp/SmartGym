from __future__ import annotations

import json
from pathlib import Path


REQUIRED_MACHINE_IDS = [
    "incline_press",
    "flat_bench_press",
    "cable_fly",
    "incline_cable_curl",
    "preacher_curl",
    "triceps_pushdown",
    "overhead_triceps_extension",
    "lat_pulldown",
    "seated_cable_row",
    "shoulder_press",
    "cable_lateral_raise",
    "face_pull",
    "leg_press",
    "calf_raise",
    "leg_extension",
    "seated_leg_curl",
    "hip_adductor",
    "hip_abductor",
]

REQUIRED_GOALS = ["hypertrophy", "strength", "endurance"]
REQUIRED_TIMING_FIELDS = [
    "targetRepsMin",
    "targetRepsMax",
    "targetSetsMin",
    "targetSetsMax",
    "targetSetsDefault",
    "restSecondsDefault",
    "riseTimeSecMin",
    "riseTimeSecMax",
    "riseTimeSecDefault",
    "lowerTimeSecMin",
    "lowerTimeSecMax",
    "lowerTimeSecDefault",
    "topPauseSec",
    "bottomPauseSec",
]

SPANISH_TERMS = [
    "hipertrofia",
    "fuerza",
    "resistencia",
    "pecho",
    "espalda",
    "hombro",
    "piernas",
    "descanso",
    "series",
    "abajo",
    "recomendada",
    "sugerido",
    "subir",
    "peso",
]


def fail(message: str) -> None:
    raise SystemExit(f"[FAIL] {message}")


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    full_seed_path = root / "firebase_seed" / "test_database_full_seed.json"
    catalog_value_path = root / "firebase_seed" / "machineCatalog_value_only.json"

    if not full_seed_path.exists():
        fail(f"Missing {full_seed_path}")
    if not catalog_value_path.exists():
        fail(f"Missing {catalog_value_path}")

    full_seed = json.loads(full_seed_path.read_text(encoding="utf-8"))
    catalog_value = json.loads(catalog_value_path.read_text(encoding="utf-8"))

    machine_catalog = full_seed.get("machineCatalog")
    if not isinstance(machine_catalog, dict):
        fail("Full seed missing top-level machineCatalog")
    machines = machine_catalog.get("machines", {})
    if not isinstance(machines, dict):
        fail("machineCatalog.machines is not an object")

    missing_machine_ids = [m for m in REQUIRED_MACHINE_IDS if m not in machines]
    if missing_machine_ids:
        fail(f"Missing machine IDs: {', '.join(missing_machine_ids)}")

    for machine_id in REQUIRED_MACHINE_IDS:
        goals = machines[machine_id].get("goals", {})
        missing_goals = [g for g in REQUIRED_GOALS if g not in goals]
        if missing_goals:
            fail(f"Machine {machine_id} missing goals: {', '.join(missing_goals)}")
        for goal_id in REQUIRED_GOALS:
            goal_payload = goals[goal_id]
            missing_fields = [f for f in REQUIRED_TIMING_FIELDS if f not in goal_payload]
            if missing_fields:
                fail(
                    f"Machine {machine_id} goal {goal_id} missing timing fields: "
                    + ", ".join(missing_fields)
                )

    users = full_seed.get("usersByRfid", {})
    if len(users) < 3:
        fail("Expected at least 3 users in usersByRfid")

    primary_uid = "7E-BA-1E-06"
    weekly = full_seed.get("athleteWeeklySessions", {}).get(primary_uid, {})
    if len(weekly) < 8:
        fail("Primary user does not have at least 8 weeks")

    primary_session_count = 0
    for week_payload in weekly.values():
        for day_payload in week_payload.get("days", {}).values():
            sessions = day_payload.get("sessions", {})
            primary_session_count += len(sessions)
            for session_payload in sessions.values():
                if "repSets" not in session_payload:
                    fail("Session missing repSets")
                if "representativeReps" not in session_payload:
                    fail("Session missing representativeReps")
    if primary_session_count < 30:
        fail("Primary user does not have at least 30 sessions")

    calibrations = full_seed.get("calibrations", {})
    primary_cal = calibrations.get(primary_uid, {})
    if "lat_pulldown" not in primary_cal:
        fail("Primary user missing lat_pulldown calibration")
    if len(primary_cal) < 4:
        fail("Primary user should have several calibration entries")

    full_seed_text = full_seed_path.read_text(encoding="utf-8").lower()
    if any(term in full_seed_text for term in SPANISH_TERMS):
        fail("Spanish terms detected in generated seed output")

    secret_markers = ["api_key", "apikey", "secret", "token", "password", "databaseurl", "firebaseio.com"]
    if any(marker in full_seed_text for marker in secret_markers):
        fail("Potential credential or private endpoint marker detected in full seed")

    if not isinstance(catalog_value, dict) or "machineCatalog" in catalog_value:
        fail("machineCatalog_value_only.json must contain only the /machineCatalog value object")

    print("[OK] test_database_full_seed.json and machineCatalog_value_only.json validated")
    print(f"[OK] users={len(users)} primary_weeks={len(weekly)} primary_sessions={primary_session_count}")


if __name__ == "__main__":
    main()
