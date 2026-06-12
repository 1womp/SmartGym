from __future__ import annotations

import json
from copy import deepcopy
from dataclasses import dataclass
from datetime import date, datetime, timedelta, timezone
from pathlib import Path


CDT = timezone(timedelta(hours=-5))


@dataclass(frozen=True)
class UserSeed:
    uid: str
    display_name: str
    goal: str
    weight_kg: float
    age: int
    height_cm: float


@dataclass(frozen=True)
class CalibrationSeed:
    machine_type_id: str
    suggested_weight_kg: float
    user_rom_percent: float


@dataclass(frozen=True)
class MachineSeed:
    machine_id: str
    machine_type_id: str
    display_name: str
    stroke_length_mm: float
    ideal_rom_percent: float
    default_calibration_weight_kg: float
    target_reps_per_set: int
    recommendations: dict[str, dict[str, float | int]]


USERS: dict[str, UserSeed] = {
    "D6-FA-A5-05": UserSeed("D6-FA-A5-05", "Usuario1", "hypertrophy", 78.0, 22, 175.0),
    "7E-BA-1E-06": UserSeed("7E-BA-1E-06", "Usuario2", "general", 84.0, 28, 181.0),
}

CALIBRATIONS: dict[str, dict[str, CalibrationSeed]] = {
    "D6-FA-A5-05": {
        "leg_ext": CalibrationSeed("leg_ext", 22.0, 91.0),
        "lat_pull": CalibrationSeed("lat_pull", 28.0, 86.0),
        "chest_press": CalibrationSeed("chest_press", 17.5, 89.0),
    },
    "7E-BA-1E-06": {
        "leg_ext": CalibrationSeed("leg_ext", 24.0, 88.0),
        "lat_pull": CalibrationSeed("lat_pull", 31.0, 83.0),
        "chest_press": CalibrationSeed("chest_press", 20.0, 86.0),
    },
}

MACHINES: dict[str, MachineSeed] = {
    "leg_ext_1": MachineSeed(
        "leg_ext_1",
        "leg_ext",
        "Leg Extension",
        420.0,
        90.0,
        20.0,
        8,
        {
            "strength": {"weightFactor": 1.10, "repsMin": 6, "repsMax": 8, "targetSets": 4, "restSeconds": 90},
            "hypertrophy": {"weightFactor": 1.00, "repsMin": 10, "repsMax": 15, "targetSets": 4, "restSeconds": 60},
            "endurance": {"weightFactor": 0.85, "repsMin": 15, "repsMax": 20, "targetSets": 3, "restSeconds": 30},
            "general": {"weightFactor": 0.95, "repsMin": 8, "repsMax": 12, "targetSets": 3, "restSeconds": 45},
            "test": {"weightFactor": 0.85, "repsMin": 4, "repsMax": 6, "targetSets": 2, "restSeconds": 30},
        },
    ),
    "lat_pull_1": MachineSeed(
        "lat_pull_1",
        "lat_pull",
        "Lat Pulldown",
        650.0,
        85.0,
        25.0,
        10,
        {
            "strength": {"weightFactor": 1.08, "repsMin": 5, "repsMax": 8, "targetSets": 4, "restSeconds": 90},
            "hypertrophy": {"weightFactor": 1.00, "repsMin": 8, "repsMax": 12, "targetSets": 4, "restSeconds": 60},
            "endurance": {"weightFactor": 0.88, "repsMin": 12, "repsMax": 18, "targetSets": 3, "restSeconds": 30},
            "general": {"weightFactor": 0.96, "repsMin": 8, "repsMax": 10, "targetSets": 3, "restSeconds": 45},
            "test": {"weightFactor": 0.85, "repsMin": 4, "repsMax": 6, "targetSets": 2, "restSeconds": 30},
        },
    ),
    "chest_press_1": MachineSeed(
        "chest_press_1",
        "chest_press",
        "Chest Press A",
        380.0,
        88.0,
        15.0,
        8,
        {
            "strength": {"weightFactor": 1.05, "repsMin": 4, "repsMax": 6, "targetSets": 5, "restSeconds": 120},
            "hypertrophy": {"weightFactor": 1.00, "repsMin": 8, "repsMax": 12, "targetSets": 4, "restSeconds": 75},
            "endurance": {"weightFactor": 0.90, "repsMin": 12, "repsMax": 16, "targetSets": 3, "restSeconds": 45},
            "general": {"weightFactor": 0.95, "repsMin": 8, "repsMax": 10, "targetSets": 3, "restSeconds": 60},
            "test": {"weightFactor": 0.85, "repsMin": 4, "repsMax": 6, "targetSets": 2, "restSeconds": 30},
        },
    ),
    "chest_press_2": MachineSeed(
        "chest_press_2",
        "chest_press",
        "Chest Press B",
        380.0,
        88.0,
        15.0,
        8,
        {
            "strength": {"weightFactor": 1.05, "repsMin": 4, "repsMax": 6, "targetSets": 5, "restSeconds": 120},
            "hypertrophy": {"weightFactor": 1.00, "repsMin": 8, "repsMax": 12, "targetSets": 4, "restSeconds": 75},
            "endurance": {"weightFactor": 0.90, "repsMin": 12, "repsMax": 16, "targetSets": 3, "restSeconds": 45},
            "general": {"weightFactor": 0.95, "repsMin": 8, "repsMax": 10, "targetSets": 3, "restSeconds": 60},
            "test": {"weightFactor": 0.85, "repsMin": 4, "repsMax": 6, "targetSets": 2, "restSeconds": 30},
        },
    ),
}

INVALID_SHORT_ROM = 1
INVALID_TOO_FAST = 2
INVALID_TOP_NOT_REACHED = 4
INVALID_NO_CONCENTRIC = 8


def iso_local(dt: datetime) -> str:
    return dt.strftime("%Y-%m-%dT%H:%M:%S%z")


def week_key_for(d: date) -> str:
    iso_year, iso_week, _ = d.isocalendar()
    return f"{iso_year}-W{iso_week:02d}"


def make_machine_config(machine: MachineSeed, version: int, updated_dt: datetime) -> dict:
    config = {
        "version": version,
        "updatedAtEpoch": int(updated_dt.timestamp()),
        "strokeLengthMm": machine.stroke_length_mm,
        "idealRomPercent": machine.ideal_rom_percent,
        "defaultCalibrationWeightKg": machine.default_calibration_weight_kg,
        "targetRepsPerSet": machine.target_reps_per_set,
    }
    for prefix, rec in machine.recommendations.items():
        config[f"{prefix}WeightFactor"] = rec["weightFactor"]
        config[f"{prefix}RepsMin"] = rec["repsMin"]
        config[f"{prefix}RepsMax"] = rec["repsMax"]
        config[f"{prefix}TargetSets"] = rec["targetSets"]
        config[f"{prefix}RestSeconds"] = rec["restSeconds"]
    return config


def make_user_profile(user: UserSeed, updated_dt: datetime) -> dict:
    return {
        "rfidUid": user.uid,
        "displayName": user.display_name,
        "hasBasicData": True,
        "weightKg": round(user.weight_kg, 2),
        "age": user.age,
        "heightCm": round(user.height_cm, 2),
        "goal": user.goal,
        "updatedAtEpoch": int(updated_dt.timestamp()),
        "updatedAtIso": iso_local(updated_dt),
    }


def make_calibration(seed: CalibrationSeed, updated_dt: datetime) -> dict:
    return {
        "machineTypeId": seed.machine_type_id,
        "hasCalibration": True,
        "suggestedWeightKg": round(seed.suggested_weight_kg, 2),
        "userRomPercent": round(seed.user_rom_percent, 2),
        "updatedAtEpoch": int(updated_dt.timestamp()),
        "updatedAtIso": iso_local(updated_dt),
    }


def count_reps_in_set(reps: list[dict], set_number: int) -> int:
    return sum(1 for rep in reps if rep["setNumber"] == set_number)


def find_set_weight_range(reps: list[dict], set_number: int) -> tuple[float, float, bool]:
    set_reps = [rep for rep in reps if rep["setNumber"] == set_number]
    if not set_reps:
        return 0.0, 0.0, False
    start = set_reps[0]["selectedWeightKg"]
    end = set_reps[-1]["selectedWeightKg"]
    return start, end, abs(start - end) > 0.01


def build_session(
    *,
    user: UserSeed,
    machine: MachineSeed,
    session_start: datetime,
    selected_weight_kg: float,
    suggested_weight_kg: float,
    user_rom_percent: float,
    session_index: int,
) -> tuple[dict, dict, dict]:
    recommendation = machine.recommendations[user.goal]
    target_sets = int(recommendation["targetSets"])
    target_reps_min = int(recommendation["repsMin"])
    target_reps_max = int(recommendation["repsMax"])
    planned_rest = int(recommendation["restSeconds"])

    session_id = f"session_{int(session_start.timestamp())}_{session_index}"

    reps: list[dict] = []
    sets: list[dict] = []
    total_rest_ms = 0
    offset_ms = 0

    valid_reps = 0
    invalid_reps = 0
    fast_warnings = 0
    rom_sum = 0.0
    concentric_sum = 0.0
    peak_sum = 0.0
    peak_ecc_sum = 0.0
    best_rom = 0.0

    for set_number in range(1, target_sets + 1):
        set_valid = 0
        set_invalid = 0
        set_fast = 0
        set_rom_sum = 0.0
        set_conc_sum = 0.0
        set_peak_sum = 0.0
        set_peak_ecc_sum = 0.0
        set_start = offset_ms

        # 1-2 repeticiones inválidas máximas en toda la sesión para verse realista.
        planned_invalid_set = 1 if (set_number == target_sets and session_index % 3 == 0) else 0
        reps_this_set = target_reps_max

        for rep_number in range(1, reps_this_set + 1):
            is_invalid = planned_invalid_set == 1 and rep_number == reps_this_set
            rom_percent = max(
                68.0,
                machine.ideal_rom_percent
                - 2.5 * (set_number - 1)
                - 0.7 * (rep_number - 1)
                + (session_index % 4)
                + (1.2 if user.uid == "D6-FA-A5-05" else -0.8),
            )
            duration_ms = 2450 + set_number * 90 + rep_number * 40 + (session_index % 5) * 15
            concentric_ms = 910 + set_number * 30 + rep_number * 12
            peak_velocity = max(44.0, 76.0 - set_number * 4.0 - rep_number * 1.1 + (session_index % 3) * 0.6)
            peak_ecc_velocity = max(48.0, 83.0 - set_number * 3.0 - rep_number * 1.0 + (session_index % 2) * 1.2)
            warning_fast = peak_ecc_velocity > 78.5 and not is_invalid
            invalid_flags = 0

            if is_invalid:
                invalid_flags = INVALID_SHORT_ROM | INVALID_TOP_NOT_REACHED
                rom_percent = max(61.0, user_rom_percent - 10.0)
                warning_fast = False

            rep = {
                "setNumber": set_number,
                "repNumberInSet": rep_number,
                "valid": not is_invalid,
                "warningFastEccentric": warning_fast,
                "selectedWeightKg": round(selected_weight_kg, 2),
                "romPercent": round(rom_percent, 2),
                "durationMs": int(duration_ms),
                "concentricTimeMs": int(concentric_ms),
                "peakVelocityPctPerSec": round(peak_velocity, 2),
                "peakEccentricVelocityPctPerSec": round(peak_ecc_velocity, 2),
                "invalidFlags": invalid_flags,
                "offsetMs": int(offset_ms),
            }
            reps.append(rep)

            offset_ms += duration_ms + 320

            if rep["valid"]:
                valid_reps += 1
                set_valid += 1
                rom_sum += rep["romPercent"]
                concentric_sum += rep["concentricTimeMs"]
                peak_sum += rep["peakVelocityPctPerSec"]
                peak_ecc_sum += rep["peakEccentricVelocityPctPerSec"]
                set_rom_sum += rep["romPercent"]
                set_conc_sum += rep["concentricTimeMs"]
                set_peak_sum += rep["peakVelocityPctPerSec"]
                set_peak_ecc_sum += rep["peakEccentricVelocityPctPerSec"]
                best_rom = max(best_rom, rep["romPercent"])
            else:
                invalid_reps += 1
                set_invalid += 1

            if rep["warningFastEccentric"]:
                fast_warnings += 1
                set_fast += 1

        actual_rest_seconds = planned_rest + ((set_number + session_index) % 3) * 4
        total_rest_ms += actual_rest_seconds * 1000
        set_end = offset_ms
        offset_ms += actual_rest_seconds * 1000

        sets.append(
            {
                "setNumber": set_number,
                "targetRepsMin": target_reps_min,
                "targetRepsMax": target_reps_max,
                "targetRepsUsed": target_reps_max,
                "validReps": set_valid,
                "invalidReps": set_invalid,
                "fastEccentricWarnings": set_fast,
                "avgRomPercent": round(set_rom_sum / set_valid, 2) if set_valid else 0.0,
                "avgConcentricTimeMs": round(set_conc_sum / set_valid, 2) if set_valid else 0.0,
                "avgPeakVelocityPctPerSec": round(set_peak_sum / set_valid, 2) if set_valid else 0.0,
                "avgPeakEccentricVelocityPctPerSec": round(set_peak_ecc_sum / set_valid, 2) if set_valid else 0.0,
                "startOffsetMs": int(set_start),
                "endOffsetMs": int(set_end),
                "plannedRestSeconds": planned_rest,
                "actualRestSeconds": actual_rest_seconds,
            }
        )

    duration_ms = offset_ms - sets[-1]["actualRestSeconds"] * 1000
    ended_at = session_start + timedelta(milliseconds=duration_ms)
    valid_rep_rate = (valid_reps * 100.0 / (valid_reps + invalid_reps)) if (valid_reps + invalid_reps) else 0.0
    avg_rom = (rom_sum / valid_reps) if valid_reps else 0.0
    avg_conc = (concentric_sum / valid_reps) if valid_reps else 0.0
    avg_peak = (peak_sum / valid_reps) if valid_reps else 0.0
    avg_peak_ecc = (peak_ecc_sum / valid_reps) if valid_reps else 0.0
    volume_load = selected_weight_kg * valid_reps
    valid_user_rom = sum(1 for rep in reps if rep["valid"] and rep["romPercent"] >= user_rom_percent)
    valid_ideal_rom = sum(1 for rep in reps if rep["valid"] and rep["romPercent"] >= machine.ideal_rom_percent)

    first_set = sets[0]
    last_set = sets[-1]
    invalid_short_rom = sum(1 for rep in reps if not rep["valid"] and rep["invalidFlags"] & INVALID_SHORT_ROM)
    invalid_too_fast = sum(1 for rep in reps if not rep["valid"] and rep["invalidFlags"] & INVALID_TOO_FAST)
    invalid_top_not_reached = sum(
        1 for rep in reps if not rep["valid"] and rep["invalidFlags"] & INVALID_TOP_NOT_REACHED
    )
    invalid_no_concentric = sum(
        1 for rep in reps if not rep["valid"] and rep["invalidFlags"] & INVALID_NO_CONCENTRIC
    )

    def rep_card(rep: dict | None) -> dict | None:
        if rep is None:
            return None
        return {
            "setNumber": rep["setNumber"],
            "repNumberInSet": rep["repNumberInSet"],
            "selectedWeightKg": rep["selectedWeightKg"],
            "romPercent": rep["romPercent"],
            "durationMs": rep["durationMs"],
            "concentricTimeMs": rep["concentricTimeMs"],
            "peakVelocityPctPerSec": rep["peakVelocityPctPerSec"],
            "peakEccentricVelocityPctPerSec": rep["peakEccentricVelocityPctPerSec"],
        }

    valid_rep_list = [rep for rep in reps if rep["valid"]]
    first_valid = valid_rep_list[0] if valid_rep_list else None
    last_valid = valid_rep_list[-1] if valid_rep_list else None
    best_rom_rep = max(valid_rep_list, key=lambda rep: rep["romPercent"]) if valid_rep_list else None
    best_vel_rep = max(valid_rep_list, key=lambda rep: rep["peakVelocityPctPerSec"]) if valid_rep_list else None

    athlete_analysis = {
        "sessionId": session_id,
        "identity": {
            "userUid": user.uid,
            "userDisplayName": user.display_name,
            "anonymous": False,
        },
        "machine": {
            "machineId": machine.machine_id,
            "machineTypeId": machine.machine_type_id,
            "machineDisplayName": machine.display_name,
            "machineIdealRomPercent": round(machine.ideal_rom_percent, 2),
        },
        "timing": {
            "startedAtEpoch": int(session_start.timestamp()),
            "endedAtEpoch": int(ended_at.timestamp()),
            "startedAtIso": iso_local(session_start),
            "endedAtIso": iso_local(ended_at),
            "startMs": int(session_start.timestamp() * 1000),
            "endMs": int(ended_at.timestamp() * 1000),
            "durationMs": int(duration_ms),
            "totalRestMs": int(total_rest_ms),
        },
        "plan": {
            "goal": user.goal,
            "calibrationBased": True,
            "selectedWeightKg": round(selected_weight_kg, 2),
            "suggestedWeightKg": round(suggested_weight_kg, 2),
            "userRomPercent": round(user_rom_percent, 2),
            "targetSets": target_sets,
            "targetRepsMin": target_reps_min,
            "targetRepsMax": target_reps_max,
            "plannedRestSeconds": planned_rest,
        },
        "summary": {
            "setsCompleted": target_sets,
            "setCount": target_sets,
            "repCount": len(reps),
            "validReps": valid_reps,
            "invalidReps": invalid_reps,
            "fastEccentricWarnings": fast_warnings,
            "validRepRate": round(valid_rep_rate, 2),
            "volumeLoadKg": round(volume_load, 2),
            "avgRomPercent": round(avg_rom, 2),
            "bestRomPercent": round(best_rom, 2),
            "avgConcentricTimeMs": round(avg_conc, 2),
            "avgPeakVelocityPctPerSec": round(avg_peak, 2),
            "avgPeakEccentricVelocityPctPerSec": round(avg_peak_ecc, 2),
        },
        "analysis": {
            "firstSetAvgRomPercent": first_set["avgRomPercent"],
            "lastSetAvgRomPercent": last_set["avgRomPercent"],
            "firstSetAvgPeakVelocityPctPerSec": first_set["avgPeakVelocityPctPerSec"],
            "lastSetAvgPeakVelocityPctPerSec": last_set["avgPeakVelocityPctPerSec"],
            "validAtOrAboveUserRomCount": valid_user_rom,
            "validBelowUserRomCount": valid_reps - valid_user_rom,
            "validAtOrAboveIdealRomCount": valid_ideal_rom,
            "romComplianceRate": round((valid_user_rom * 100.0 / valid_reps) if valid_reps else 0.0, 2),
            "idealRomHitRate": round((valid_ideal_rom * 100.0 / valid_reps) if valid_reps else 0.0, 2),
            "avgRestSecondsPerSet": round(total_rest_ms / 1000.0 / target_sets, 2),
            "fatigueRomDrop": round(first_set["avgRomPercent"] - last_set["avgRomPercent"], 2),
            "fatigueVelocityDrop": round(
                first_set["avgPeakVelocityPctPerSec"] - last_set["avgPeakVelocityPctPerSec"], 2
            ),
            "invalidShortRomCount": invalid_short_rom,
            "invalidTooFastCount": invalid_too_fast,
            "invalidTopNotReachedCount": invalid_top_not_reached,
            "invalidNoConcentricCount": invalid_no_concentric,
        },
        "setOverview": [],
        "paths": {"setDetails": "setDetails", "repSets": "repSets"},
        "representativeReps": {
            "firstValid": rep_card(first_valid),
            "bestRom": rep_card(best_rom_rep),
            "bestVelocity": rep_card(best_vel_rep),
            "lastValid": rep_card(last_valid),
        },
    }

    set_details = {"sessionId": session_id, "setCount": target_sets, "sets": {}}
    rep_sets: dict[str, dict] = {}

    for set_record in sets:
        set_number = set_record["setNumber"]
        weight_start, weight_end, weight_changed = find_set_weight_range(reps, set_number)
        rep_count = count_reps_in_set(reps, set_number)
        overview = {
            "setNumber": set_number,
            "repCount": rep_count,
            "targetRepsUsed": set_record["targetRepsUsed"],
            "validReps": set_record["validReps"],
            "invalidReps": set_record["invalidReps"],
            "fastEccentricWarnings": set_record["fastEccentricWarnings"],
            "selectedWeightKgStart": round(weight_start, 2),
            "selectedWeightKgEnd": round(weight_end, 2),
            "weightChangedDuringSet": weight_changed,
            "avgRomPercent": set_record["avgRomPercent"],
            "avgConcentricTimeMs": set_record["avgConcentricTimeMs"],
            "avgPeakVelocityPctPerSec": set_record["avgPeakVelocityPctPerSec"],
            "avgPeakEccentricVelocityPctPerSec": set_record["avgPeakEccentricVelocityPctPerSec"],
            "plannedRestSeconds": set_record["plannedRestSeconds"],
            "actualRestSeconds": set_record["actualRestSeconds"],
        }
        athlete_analysis["setOverview"].append(overview)

        set_detail = deepcopy(set_record)
        set_detail["repCount"] = rep_count
        set_detail["selectedWeightKgStart"] = round(weight_start, 2)
        set_detail["selectedWeightKgEnd"] = round(weight_end, 2)
        set_detail["weightChangedDuringSet"] = weight_changed
        set_details["sets"][f"set{set_number}"] = set_detail

        set_reps = []
        for idx, rep in enumerate(reps):
            if rep["setNumber"] != set_number:
                continue
            rep_payload = deepcopy(rep)
            rep_payload["index"] = idx
            set_reps.append(rep_payload)

        rep_sets[f"set{set_number}"] = {
            "sessionId": session_id,
            "setNumber": set_number,
            "count": rep_count,
            "setSummary": overview,
            "reps": set_reps,
        }

    session_payload = athlete_analysis | {
        "setDetails": set_details,
        "repSets": rep_sets,
    }

    day_key = session_start.date().isoformat()
    week_key = week_key_for(session_start.date())

    timeline = {
        "sessionId": session_id,
        "dayKey": day_key,
        "weekKey": week_key,
        "identity": {"userUid": user.uid, "userDisplayName": user.display_name},
        "ordering": {
            "startedAtEpoch": int(session_start.timestamp()),
            "endedAtEpoch": int(ended_at.timestamp()),
            "startedAtIso": iso_local(session_start),
            "endedAtIso": iso_local(ended_at),
        },
        "machine": {
            "machineId": machine.machine_id,
            "machineTypeId": machine.machine_type_id,
            "machineDisplayName": machine.display_name,
        },
        "plan": {
            "goal": user.goal,
            "selectedWeightKg": round(selected_weight_kg, 2),
            "targetSets": target_sets,
            "targetRepsMin": target_reps_min,
            "targetRepsMax": target_reps_max,
        },
        "summary": {
            "setsCompleted": target_sets,
            "validReps": valid_reps,
            "invalidReps": invalid_reps,
            "avgRomPercent": round(avg_rom, 2),
            "durationMs": int(duration_ms),
        },
    }

    meta = {
        "dayKey": day_key,
        "weekKey": week_key,
        "userUid": user.uid,
        "userDisplayName": user.display_name,
        "lastSessionId": session_id,
        "lastStartedAtEpoch": int(session_start.timestamp()),
        "lastEndedAtEpoch": int(ended_at.timestamp()),
        "lastStartedAtIso": iso_local(session_start),
        "lastEndedAtIso": iso_local(ended_at),
        "lastMachine": {
            "machineId": machine.machine_id,
            "machineTypeId": machine.machine_type_id,
            "machineDisplayName": machine.display_name,
        },
    }
    return session_payload, timeline, meta


def update_summary(summary: dict | None, session_data: dict, machine_type_id: str, goal: str, scope: str) -> dict:
    timing = session_data["timing"]
    summ = session_data["summary"]
    if summary is None:
        summary = {
            "scope": scope,
            "lastSessionId": session_data["sessionId"],
            "lastStartedAtEpoch": timing["startedAtEpoch"],
            "lastStartedAtIso": timing["startedAtIso"],
            "totalSessions": 0,
            "totalValidReps": 0,
            "totalInvalidReps": 0,
            "totalSetsCompleted": 0,
            "totalDurationMs": 0,
            "totalRestMs": 0,
            "totalFastEccentricWarnings": 0,
            "totalVolumeLoadKg": 0.0,
            "avgRomPercent": 0.0,
            "avgPeakVelocityPctPerSec": 0.0,
            "machineTypeCounts": {},
            "goalCounts": {},
        }

    prev_valid = summary["totalValidReps"]
    rom_weighted = summary["avgRomPercent"] * prev_valid
    vel_weighted = summary["avgPeakVelocityPctPerSec"] * prev_valid

    summary["lastSessionId"] = session_data["sessionId"]
    summary["lastStartedAtEpoch"] = timing["startedAtEpoch"]
    summary["lastStartedAtIso"] = timing["startedAtIso"]
    summary["totalSessions"] += 1
    summary["totalValidReps"] += summ["validReps"]
    summary["totalInvalidReps"] += summ["invalidReps"]
    summary["totalSetsCompleted"] += summ["setsCompleted"]
    summary["totalDurationMs"] += timing["durationMs"]
    summary["totalRestMs"] += timing["totalRestMs"]
    summary["totalFastEccentricWarnings"] += summ["fastEccentricWarnings"]
    summary["totalVolumeLoadKg"] = round(summary["totalVolumeLoadKg"] + summ["volumeLoadKg"], 2)

    if summary["totalValidReps"] > 0:
        summary["avgRomPercent"] = round(
            (rom_weighted + summ["avgRomPercent"] * summ["validReps"]) / summary["totalValidReps"], 2
        )
        summary["avgPeakVelocityPctPerSec"] = round(
            (vel_weighted + summ["avgPeakVelocityPctPerSec"] * summ["validReps"]) / summary["totalValidReps"], 2
        )

    summary["machineTypeCounts"][machine_type_id] = summary["machineTypeCounts"].get(machine_type_id, 0) + 1
    summary["goalCounts"][goal] = summary["goalCounts"].get(goal, 0) + 1
    return summary


def build_export() -> dict:
    export = {
        "usersByRfid": {},
        "calibrations": {},
        "calibrationHistory": {},
        "machineConfigs": {},
        "devices": {},
        "athleteWeeklySessions": {},
    }

    start_day = date(2026, 4, 13)
    updated_dt = datetime(2026, 4, 20, 8, 30, 0, tzinfo=CDT)

    for uid, user in USERS.items():
        export["usersByRfid"][uid] = make_user_profile(user, updated_dt)
        export["calibrations"][uid] = {}
        export["calibrationHistory"][uid] = {}
        for machine_type_id, calibration in CALIBRATIONS[uid].items():
            payload = make_calibration(calibration, updated_dt)
            export["calibrations"][uid][machine_type_id] = payload
            export["calibrationHistory"][uid][machine_type_id] = {str(int(updated_dt.timestamp())): payload}

    for idx, machine in enumerate(MACHINES.values(), start=1):
        export["machineConfigs"][machine.machine_id] = make_machine_config(machine, idx, updated_dt)

    export["devices"]["94-A9-90-11-9A-B4"] = {
        "deviceId": "94-A9-90-11-9A-B4",
        "macAddress": "94:A9:90:11:9A:B4",
        "ipAddress": "192.168.1.88",
        "machineId": "leg_ext_1",
        "machineTypeId": "leg_ext",
        "machineDisplayName": "Leg Extension",
        "appState": "READY",
        "activeUserUid": "D6-FA-A5-05",
        "wifiConnected": True,
        "updatedAtEpoch": int(updated_dt.timestamp()),
        "updatedAtIso": iso_local(updated_dt),
    }

    schedule = [
        (date(2026, 4, 13), "D6-FA-A5-05", "leg_ext_1", 7, 10, 23.0),
        (date(2026, 4, 14), "7E-BA-1E-06", "lat_pull_1", 18, 20, 30.0),
        (date(2026, 4, 15), "D6-FA-A5-05", "chest_press_1", 8, 5, 17.5),
        (date(2026, 4, 16), "7E-BA-1E-06", "leg_ext_1", 17, 45, 24.0),
        (date(2026, 4, 17), "D6-FA-A5-05", "lat_pull_1", 7, 20, 28.0),
        (date(2026, 4, 18), "7E-BA-1E-06", "chest_press_2", 19, 0, 20.0),
        (date(2026, 4, 19), "D6-FA-A5-05", "leg_ext_1", 8, 15, 22.5),
        (date(2026, 4, 20), "7E-BA-1E-06", "lat_pull_1", 18, 10, 31.0),
        (date(2026, 4, 21), "D6-FA-A5-05", "chest_press_1", 7, 35, 18.0),
        (date(2026, 4, 22), "7E-BA-1E-06", "leg_ext_1", 17, 20, 25.0),
        (date(2026, 4, 23), "D6-FA-A5-05", "lat_pull_1", 8, 0, 29.0),
        (date(2026, 4, 24), "7E-BA-1E-06", "chest_press_2", 11, 15, 20.0),
        (date(2026, 4, 25), "D6-FA-A5-05", "leg_ext_1", 9, 10, 23.5),
        (date(2026, 4, 26), "7E-BA-1E-06", "lat_pull_1", 10, 45, 30.5),
    ]

    for session_index, (session_day, uid, machine_id, hour, minute, selected_weight) in enumerate(schedule, start=1):
        start_dt = datetime(session_day.year, session_day.month, session_day.day, hour, minute, 0, tzinfo=CDT)
        week_key = week_key_for(session_day)
        user = USERS[uid]
        machine = MACHINES[machine_id]
        calibration = CALIBRATIONS[uid][machine.machine_type_id]

        session_payload, timeline, meta = build_session(
            user=user,
            machine=machine,
            session_start=start_dt,
            selected_weight_kg=selected_weight,
            suggested_weight_kg=calibration.suggested_weight_kg,
            user_rom_percent=calibration.user_rom_percent,
            session_index=session_index,
        )

        user_week = export["athleteWeeklySessions"].setdefault(uid, {}).setdefault(week_key, {"days": {}})
        day_key = start_dt.date().isoformat()
        day_bucket = user_week["days"].setdefault(day_key, {"meta": meta, "timeline": {}, "sessions": {}})
        day_bucket["meta"] = meta
        day_bucket["timeline"][f"{timeline['ordering']['startedAtEpoch']}_{session_payload['sessionId']}"] = timeline

        session_id = session_payload["sessionId"]
        stored_session = deepcopy(session_payload)
        set_details = stored_session.pop("setDetails")
        rep_sets = stored_session.pop("repSets")
        stored_session["setDetails"] = set_details
        stored_session["repSets"] = rep_sets
        day_bucket["sessions"][session_id] = stored_session

        day_bucket["daySummary"] = update_summary(
            day_bucket.get("daySummary"),
            session_payload,
            machine.machine_type_id,
            user.goal,
            "day",
        )
        user_week["weekSummary"] = update_summary(
            user_week.get("weekSummary"),
            session_payload,
            machine.machine_type_id,
            user.goal,
            "week",
        )

    return export


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    output_dir = root / "sample_data"
    output_dir.mkdir(exist_ok=True)
    export = build_export()
    primary_path = output_dir / "firebase_rtdb_two_week_seed_2026_w16_w17.json"
    legacy_path = output_dir / "firebase_rtdb_week_seed_2026_w16.json"
    payload = json.dumps(export, indent=2, ensure_ascii=False)
    primary_path.write_text(payload, encoding="utf-8")
    legacy_path.write_text(payload, encoding="utf-8")
    print(primary_path)
    print(legacy_path)


if __name__ == "__main__":
    main()
