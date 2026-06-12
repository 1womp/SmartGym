from __future__ import annotations

import json
import math
import random
from collections import defaultdict
from copy import deepcopy
from dataclasses import dataclass
from datetime import date, datetime, time, timedelta, timezone
from pathlib import Path


TZ = timezone(timedelta(hours=-5))
RNG = random.Random(20260424)


@dataclass(frozen=True)
class Machine:
    machine_id: str
    machine_type_id: str
    display_name: str
    exercise_category: str
    primary_muscle_group: str
    secondary_muscle_group: str
    stroke_length_mm: float
    ideal_rom_percent: float
    default_calibration_weight_kg: float
    target_reps_per_set: int
    recs: dict[str, dict[str, float | int]]


MACHINES = [
    Machine(
        "leg_ext_1",
        "leg_ext",
        "Leg Extension",
        "strength_machine",
        "quadriceps",
        "knees",
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
    Machine(
        "lat_pull_1",
        "lat_pull",
        "Lat Pulldown",
        "cable_machine",
        "lats",
        "upper_back",
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
    Machine(
        "chest_press_1",
        "chest_press",
        "Chest Press A",
        "plate_loaded_machine",
        "chest",
        "triceps",
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
    Machine(
        "chest_press_2",
        "chest_press",
        "Chest Press B",
        "plate_loaded_machine",
        "chest",
        "triceps",
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
    Machine(
        "seated_row_1",
        "seated_row",
        "Seated Row",
        "cable_machine",
        "upper_back",
        "biceps",
        540.0,
        87.0,
        22.5,
        10,
        {
            "strength": {"weightFactor": 1.10, "repsMin": 6, "repsMax": 8, "targetSets": 4, "restSeconds": 90},
            "hypertrophy": {"weightFactor": 1.00, "repsMin": 8, "repsMax": 12, "targetSets": 4, "restSeconds": 60},
            "endurance": {"weightFactor": 0.88, "repsMin": 12, "repsMax": 18, "targetSets": 3, "restSeconds": 30},
            "general": {"weightFactor": 0.96, "repsMin": 8, "repsMax": 12, "targetSets": 3, "restSeconds": 45},
            "test": {"weightFactor": 0.85, "repsMin": 4, "repsMax": 6, "targetSets": 2, "restSeconds": 30},
        },
    ),
]

USERS = [
    {"uid": "D6-FA-A5-05", "name": "Ariana Lopez", "weightKg": 78.0, "age": 22, "heightCm": 175.0, "goal": "hypertrophy"},
    {"uid": "7E-BA-1E-06", "name": "Diego Ramos", "weightKg": 84.0, "age": 28, "heightCm": 181.0, "goal": "general"},
    {"uid": "91-C4-2A-11", "name": "Sofia Martinez", "weightKg": 63.5, "age": 25, "heightCm": 168.0, "goal": "endurance"},
    {"uid": "3B-8D-44-22", "name": "Mateo Cruz", "weightKg": 91.2, "age": 31, "heightCm": 183.0, "goal": "strength"},
    {"uid": "AF-73-9C-40", "name": "Valeria Torres", "weightKg": 70.4, "age": 27, "heightCm": 172.0, "goal": "hypertrophy"},
]


def iso_string(dt: datetime) -> str:
    return dt.strftime("%Y-%m-%dT%H:%M:%S-0500")


def epoch(dt: datetime) -> int:
    return int(dt.timestamp())


def day_key(d: date) -> str:
    return d.isoformat()


def week_key(d: date) -> str:
    iso = d.isocalendar()
    return f"{iso.year}-W{iso.week:02d}"


def round2(value: float) -> float:
    return round(value + 1e-9, 2)


def build_machine_configs(base_epoch: int) -> dict:
    configs = {}
    for index, machine in enumerate(MACHINES, start=1):
        cfg = {
            "machineId": machine.machine_id,
            "exerciseCategory": machine.exercise_category,
            "primaryMuscleGroup": machine.primary_muscle_group,
            "secondaryMuscleGroup": machine.secondary_muscle_group,
            "version": index,
            "updatedAtEpoch": base_epoch,
            "strokeLengthMm": machine.stroke_length_mm,
            "idealRomPercent": machine.ideal_rom_percent,
            "defaultCalibrationWeightKg": machine.default_calibration_weight_kg,
            "targetRepsPerSet": machine.target_reps_per_set,
            "encoderCalibrationValid": index <= 2,
            "encoderZeroRaw": 1180 + index * 14 if index <= 2 else 0,
            "encoderFullRaw": 2840 + index * 18 if index <= 2 else 0,
            "encoderReferenceDistanceMm": 1000.0,
            "encoderInvertDirection": False,
        }
        for goal, rec in machine.recs.items():
            cfg[f"{goal}WeightFactor"] = rec["weightFactor"]
            cfg[f"{goal}RepsMin"] = rec["repsMin"]
            cfg[f"{goal}RepsMax"] = rec["repsMax"]
            cfg[f"{goal}TargetSets"] = rec["targetSets"]
            cfg[f"{goal}RestSeconds"] = rec["restSeconds"]
        configs[machine.machine_id] = cfg
    return configs


def build_users(base_dt: datetime) -> dict:
    users = {}
    for user in USERS:
        users[user["uid"]] = {
            "rfidUid": user["uid"],
            "displayName": user["name"],
            "hasBasicData": True,
            "weightKg": user["weightKg"],
            "age": user["age"],
            "heightCm": user["heightCm"],
            "goal": user["goal"],
            "updatedAtEpoch": epoch(base_dt),
            "updatedAtIso": iso_string(base_dt),
        }
    return users


def build_calibrations(base_dt: datetime) -> tuple[dict, dict]:
    calibrations = {}
    history = {}
    for user_index, user in enumerate(USERS):
        user_cal = {}
        user_hist = {}
        for machine_index, machine in enumerate(MACHINES):
            suggested = machine.default_calibration_weight_kg + 2.5 * (user_index * 0.5 + machine_index * 0.3)
            rom = machine.ideal_rom_percent - RNG.uniform(0.5, 5.0)
            record = {
                "machineTypeId": machine.machine_type_id,
                "hasCalibration": True,
                "suggestedWeightKg": round2(suggested),
                "userRomPercent": round2(rom),
                "updatedAtEpoch": epoch(base_dt),
                "updatedAtIso": iso_string(base_dt),
            }
            user_cal[machine.machine_type_id] = record
            history_points = {}
            for step in range(3):
                dt = base_dt - timedelta(days=(14 - step * 7))
                history_points[str(epoch(dt))] = {
                    "machineTypeId": machine.machine_type_id,
                    "hasCalibration": True,
                    "suggestedWeightKg": round2(suggested - 0.5 + step * 0.25),
                    "userRomPercent": round2(rom - 1.5 + step * 0.7),
                    "updatedAtEpoch": epoch(dt),
                    "updatedAtIso": iso_string(dt),
                }
            user_hist[machine.machine_type_id] = history_points
        calibrations[user["uid"]] = user_cal
        history[user["uid"]] = user_hist
    return calibrations, history


def body_metrics_for_user(user: dict, start_day: date, weeks: int) -> dict:
    metrics = {}
    base_weight = user["weightKg"]
    for week in range(weeks):
        for offset in (0, 3):
            current_day = start_day + timedelta(days=week * 7 + offset)
            weight_delta = (week * -0.15) if user["goal"] in ("hypertrophy", "strength") else (week * -0.05)
            checkin_weight = base_weight + weight_delta + RNG.uniform(-0.4, 0.4)
            body_fat = 16.0 + (user["age"] % 5) + RNG.uniform(-0.8, 0.8)
            metrics[day_key(current_day)] = {
                "dateKey": day_key(current_day),
                "weightKg": round2(checkin_weight),
                "bodyFatPercent": round2(body_fat),
                "skeletalMusclePercent": round2(39.0 + RNG.uniform(-1.0, 2.0)),
                "waistCm": round2(78.0 + RNG.uniform(-2.0, 3.0)),
                "sleepHours": round2(6.8 + RNG.uniform(-1.0, 1.2)),
                "energyScore": RNG.randint(3, 5),
                "notes": [
                    "Recovery trending up",
                    "Busy week but training stayed consistent",
                    "Felt stronger on compound machine work",
                    "Slight fatigue mid-week, recovered by weekend",
                ][(week + offset) % 4],
                "updatedAtEpoch": epoch(datetime.combine(current_day, time(7, 30), TZ)),
                "updatedAtIso": iso_string(datetime.combine(current_day, time(7, 30), TZ)),
            }
    return metrics


def build_rep(rep_index: int, set_number: int, selected_weight: float, user_rom: float, ideal_rom: float, offset_ms: int) -> dict:
    valid = RNG.random() > 0.08
    rom = RNG.uniform(user_rom - 5.5, ideal_rom + 4.5)
    duration_ms = RNG.randint(1800, 3200)
    concentric_ms = RNG.randint(700, 1500)
    peak_velocity = RNG.uniform(42.0, 78.0)
    peak_ecc_velocity = peak_velocity + RNG.uniform(3.0, 12.0)
    invalid_flags = 0
    if not valid:
        invalid_flags = RNG.choice([1, 2, 4, 8])
    return {
        "index": rep_index - 1,
        "setNumber": set_number,
        "repNumberInSet": rep_index,
        "valid": valid,
        "warningFastEccentric": peak_ecc_velocity > 72.0,
        "selectedWeightKg": round2(selected_weight),
        "romPercent": round2(max(35.0, rom)),
        "durationMs": duration_ms,
        "concentricTimeMs": concentric_ms,
        "peakVelocityPctPerSec": round2(peak_velocity),
        "peakEccentricVelocityPctPerSec": round2(peak_ecc_velocity),
        "invalidFlags": invalid_flags,
        "offsetMs": offset_ms,
    }


def build_session(user: dict, machine: Machine, calibration: dict, start_dt: datetime, session_number: int) -> tuple[str, dict, dict]:
    goal = user["goal"]
    rec = machine.recs[goal]
    target_sets = int(rec["targetSets"])
    target_reps_min = int(rec["repsMin"])
    target_reps_max = int(rec["repsMax"])
    planned_rest = int(rec["restSeconds"])
    selected_weight = calibration["suggestedWeightKg"] * rec["weightFactor"]
    selected_weight = round2(selected_weight + RNG.choice([-2.5, 0.0, 0.0, 2.5]))
    set_count = max(2, target_sets - (1 if RNG.random() < 0.15 else 0))

    rep_sets = {}
    set_overview = []
    set_details = {}
    all_reps = []
    session_offset = 0
    total_rest_ms = 0

    first_set_rom = 0.0
    last_set_rom = 0.0
    first_set_vel = 0.0
    last_set_vel = 0.0
    valid_above_user = 0
    valid_below_user = 0
    valid_above_ideal = 0
    invalid_short = 0
    invalid_fast = 0
    invalid_top = 0
    invalid_no_con = 0
    valid_count = 0
    invalid_count = 0
    fast_ecc = 0
    rom_sum = 0.0
    conc_sum = 0.0
    peak_sum = 0.0
    peak_ecc_sum = 0.0
    best_rom = 0.0

    first_valid = None
    best_rom_rep = None
    best_vel_rep = None
    last_valid = None
    best_vel_seen = -1.0

    for set_number in range(1, set_count + 1):
        target_used = RNG.randint(target_reps_min, target_reps_max)
        rep_count = max(target_reps_min - 1, target_used - RNG.randint(0, 1))
        reps = []
        set_valid = 0
        set_invalid = 0
        set_fast = 0
        set_rom_sum = 0.0
        set_conc_sum = 0.0
        set_peak_sum = 0.0
        set_peak_ecc_sum = 0.0
        set_start_offset = session_offset

        for rep_number in range(1, rep_count + 1):
            rep = build_rep(rep_number, set_number, selected_weight, calibration["userRomPercent"], machine.ideal_rom_percent, session_offset)
            reps.append(rep)
            all_reps.append(rep)
            session_offset += rep["durationMs"] + RNG.randint(900, 2100)

            if rep["valid"]:
                valid_count += 1
                set_valid += 1
                rom_sum += rep["romPercent"]
                conc_sum += rep["concentricTimeMs"]
                peak_sum += rep["peakVelocityPctPerSec"]
                peak_ecc_sum += rep["peakEccentricVelocityPctPerSec"]
                set_rom_sum += rep["romPercent"]
                set_conc_sum += rep["concentricTimeMs"]
                set_peak_sum += rep["peakVelocityPctPerSec"]
                set_peak_ecc_sum += rep["peakEccentricVelocityPctPerSec"]
                best_rom = max(best_rom, rep["romPercent"])
                if rep["romPercent"] >= calibration["userRomPercent"]:
                    valid_above_user += 1
                else:
                    valid_below_user += 1
                if rep["romPercent"] >= machine.ideal_rom_percent:
                    valid_above_ideal += 1
                if first_valid is None:
                    first_valid = rep
                last_valid = rep
                if best_rom_rep is None or rep["romPercent"] > best_rom_rep["romPercent"]:
                    best_rom_rep = rep
                if rep["peakVelocityPctPerSec"] > best_vel_seen:
                    best_vel_seen = rep["peakVelocityPctPerSec"]
                    best_vel_rep = rep
            else:
                invalid_count += 1
                set_invalid += 1
                flag = rep["invalidFlags"]
                if flag == 1:
                    invalid_short += 1
                elif flag == 2:
                    invalid_fast += 1
                elif flag == 4:
                    invalid_top += 1
                else:
                    invalid_no_con += 1

            if rep["warningFastEccentric"]:
                fast_ecc += 1
                set_fast += 1

        avg_rom = round2(set_rom_sum / set_valid) if set_valid else 0.0
        avg_peak = round2(set_peak_sum / set_valid) if set_valid else 0.0
        avg_peak_ecc = round2(set_peak_ecc_sum / set_valid) if set_valid else 0.0
        avg_conc = round2(set_conc_sum / set_valid) if set_valid else 0.0
        actual_rest = planned_rest + RNG.randint(-8, 16)
        total_rest_ms += max(0, actual_rest) * 1000
        session_offset += max(0, actual_rest) * 1000
        set_end_offset = session_offset

        if set_number == 1:
            first_set_rom = avg_rom
            first_set_vel = avg_peak
        last_set_rom = avg_rom
        last_set_vel = avg_peak

        set_summary = {
            "setNumber": set_number,
            "repCount": len(reps),
            "targetRepsUsed": target_used,
            "validReps": set_valid,
            "invalidReps": set_invalid,
            "fastEccentricWarnings": set_fast,
            "selectedWeightKgStart": round2(selected_weight),
            "selectedWeightKgEnd": round2(selected_weight),
            "weightChangedDuringSet": False,
            "avgRomPercent": avg_rom,
            "avgConcentricTimeMs": avg_conc,
            "avgPeakVelocityPctPerSec": avg_peak,
            "avgPeakEccentricVelocityPctPerSec": avg_peak_ecc,
            "plannedRestSeconds": planned_rest,
            "actualRestSeconds": max(0, actual_rest),
        }
        set_overview.append(set_summary)
        set_details[f"set{set_number}"] = {
            "setNumber": set_number,
            "targetRepsMin": target_reps_min,
            "targetRepsMax": target_reps_max,
            "targetRepsUsed": target_used,
            "validReps": set_valid,
            "invalidReps": set_invalid,
            "fastEccentricWarnings": set_fast,
            "selectedWeightKgStart": round2(selected_weight),
            "selectedWeightKgEnd": round2(selected_weight),
            "weightChangedDuringSet": False,
            "avgRomPercent": avg_rom,
            "avgConcentricTimeMs": avg_conc,
            "avgPeakVelocityPctPerSec": avg_peak,
            "avgPeakEccentricVelocityPctPerSec": avg_peak_ecc,
            "startOffsetMs": set_start_offset,
            "endOffsetMs": set_end_offset,
            "plannedRestSeconds": planned_rest,
            "actualRestSeconds": max(0, actual_rest),
        }
        rep_sets[f"set{set_number}"] = {
            "sessionId": "",
            "setNumber": set_number,
            "count": len(reps),
            "setSummary": deepcopy(set_summary),
            "reps": reps,
        }

    duration_ms = session_offset
    end_dt = start_dt + timedelta(milliseconds=duration_ms)
    session_id = f"session_{epoch(start_dt)}_{session_number}"
    for rep_set in rep_sets.values():
        rep_set["sessionId"] = session_id

    volume_load = round2(selected_weight * valid_count)
    valid_rep_rate = round2((valid_count * 100.0) / max(1, valid_count + invalid_count))
    rom_compliance = round2((valid_above_user * 100.0) / max(1, valid_count))
    ideal_rom_hit_rate = round2((valid_above_ideal * 100.0) / max(1, valid_count))
    avg_rest_sec = round2(total_rest_ms / 1000.0 / max(1, set_count))

    session = {
        "sessionId": session_id,
        "identity": {
            "userUid": user["uid"],
            "userDisplayName": user["name"],
            "anonymous": False,
        },
        "machine": {
            "machineId": machine.machine_id,
            "machineTypeId": machine.machine_type_id,
            "machineDisplayName": machine.display_name,
            "exerciseCategory": machine.exercise_category,
            "primaryMuscleGroup": machine.primary_muscle_group,
            "secondaryMuscleGroup": machine.secondary_muscle_group,
            "machineIdealRomPercent": machine.ideal_rom_percent,
        },
        "timing": {
            "startedAtEpoch": epoch(start_dt),
            "endedAtEpoch": epoch(end_dt),
            "startedAtIso": iso_string(start_dt),
            "endedAtIso": iso_string(end_dt),
            "startMs": epoch(start_dt) * 1000,
            "endMs": epoch(end_dt) * 1000,
            "durationMs": duration_ms,
            "totalRestMs": total_rest_ms,
        },
        "plan": {
            "goal": goal,
            "calibrationBased": True,
            "selectedWeightKg": round2(selected_weight),
            "suggestedWeightKg": calibration["suggestedWeightKg"],
            "userRomPercent": calibration["userRomPercent"],
            "targetSets": target_sets,
            "targetRepsMin": target_reps_min,
            "targetRepsMax": target_reps_max,
            "plannedRestSeconds": planned_rest,
        },
        "summary": {
            "setsCompleted": set_count,
            "setCount": set_count,
            "repCount": len(all_reps),
            "validReps": valid_count,
            "invalidReps": invalid_count,
            "fastEccentricWarnings": fast_ecc,
            "validRepRate": valid_rep_rate,
            "volumeLoadKg": volume_load,
            "avgRomPercent": round2(rom_sum / max(1, valid_count)),
            "bestRomPercent": round2(best_rom),
            "avgConcentricTimeMs": round2(conc_sum / max(1, valid_count)),
            "avgPeakVelocityPctPerSec": round2(peak_sum / max(1, valid_count)),
            "avgPeakEccentricVelocityPctPerSec": round2(peak_ecc_sum / max(1, valid_count)),
        },
        "analysis": {
            "firstSetAvgRomPercent": round2(first_set_rom),
            "lastSetAvgRomPercent": round2(last_set_rom),
            "firstSetAvgPeakVelocityPctPerSec": round2(first_set_vel),
            "lastSetAvgPeakVelocityPctPerSec": round2(last_set_vel),
            "validAtOrAboveUserRomCount": valid_above_user,
            "validBelowUserRomCount": valid_below_user,
            "validAtOrAboveIdealRomCount": valid_above_ideal,
            "romComplianceRate": rom_compliance,
            "idealRomHitRate": ideal_rom_hit_rate,
            "avgRestSecondsPerSet": avg_rest_sec,
            "fatigueRomDrop": round2(first_set_rom - last_set_rom),
            "fatigueVelocityDrop": round2(first_set_vel - last_set_vel),
            "invalidShortRomCount": invalid_short,
            "invalidTooFastCount": invalid_fast,
            "invalidTopNotReachedCount": invalid_top,
            "invalidNoConcentricCount": invalid_no_con,
        },
        "setOverview": set_overview,
        "paths": {
            "setDetails": "setDetails",
            "repSets": "repSets",
        },
        "representativeReps": {
            "firstValid": first_valid,
            "bestRom": best_rom_rep,
            "bestVelocity": best_vel_rep,
            "lastValid": last_valid,
        },
        "setDetails": {
            "sessionId": session_id,
            "setCount": set_count,
            "sets": set_details,
        },
        "repSets": rep_sets,
    }

    timeline = {
        "sessionId": session_id,
        "dayKey": day_key(start_dt.date()),
        "weekKey": week_key(start_dt.date()),
        "identity": {
            "userUid": user["uid"],
            "userDisplayName": user["name"],
        },
        "ordering": {
            "startedAtEpoch": epoch(start_dt),
            "endedAtEpoch": epoch(end_dt),
            "startedAtIso": iso_string(start_dt),
            "endedAtIso": iso_string(end_dt),
        },
        "machine": {
            "machineId": machine.machine_id,
            "machineTypeId": machine.machine_type_id,
            "machineDisplayName": machine.display_name,
            "exerciseCategory": machine.exercise_category,
            "primaryMuscleGroup": machine.primary_muscle_group,
            "secondaryMuscleGroup": machine.secondary_muscle_group,
        },
        "plan": {
            "goal": goal,
            "selectedWeightKg": round2(selected_weight),
            "targetSets": target_sets,
            "targetRepsMin": target_reps_min,
            "targetRepsMax": target_reps_max,
        },
        "summary": {
            "setsCompleted": set_count,
            "validReps": valid_count,
            "invalidReps": invalid_count,
            "avgRomPercent": session["summary"]["avgRomPercent"],
            "durationMs": duration_ms,
        },
    }
    return session_id, session, timeline


def aggregate_summary(timelines: list[dict], scope: str) -> dict:
    total_sessions = len(timelines)
    total_valid = sum(item["summary"]["validReps"] for item in timelines)
    total_invalid = sum(item["summary"]["invalidReps"] for item in timelines)
    total_sets = sum(item["summary"]["setsCompleted"] for item in timelines)
    total_duration = sum(item["summary"]["durationMs"] for item in timelines)
    total_volume = round2(sum(item["plan"]["selectedWeightKg"] * item["summary"]["validReps"] for item in timelines))
    rom_weighted = sum(item["summary"]["avgRomPercent"] * item["summary"]["validReps"] for item in timelines)
    avg_rom = round2(rom_weighted / max(1, total_valid))
    machine_counts: dict[str, int] = defaultdict(int)
    goal_counts: dict[str, int] = defaultdict(int)
    for item in timelines:
        machine_counts[item["machine"]["machineTypeId"]] += 1
        goal_counts[item["plan"]["goal"]] += 1
    last_item = sorted(timelines, key=lambda x: x["ordering"]["startedAtEpoch"])[-1]
    return {
        "scope": scope,
        "lastSessionId": last_item["sessionId"],
        "lastStartedAtEpoch": last_item["ordering"]["startedAtEpoch"],
        "lastStartedAtIso": last_item["ordering"]["startedAtIso"],
        "totalSessions": total_sessions,
        "totalValidReps": total_valid,
        "totalInvalidReps": total_invalid,
        "totalSetsCompleted": total_sets,
        "totalDurationMs": total_duration,
        "totalRestMs": 0,
        "totalFastEccentricWarnings": 0,
        "totalVolumeLoadKg": total_volume,
        "avgRomPercent": avg_rom,
        "avgPeakVelocityPctPerSec": round2(59.0 + RNG.uniform(-4.0, 6.0)),
        "machineTypeCounts": dict(sorted(machine_counts.items())),
        "goalCounts": dict(sorted(goal_counts.items())),
    }


def build_seed() -> dict:
    start_day = date(2026, 3, 16)
    weeks = 6
    base_dt = datetime(2026, 4, 24, 8, 30, tzinfo=TZ)

    seed = {
        "usersByRfid": build_users(base_dt),
        "machineConfigs": build_machine_configs(epoch(base_dt)),
        "devices": {},
        "bodyMetricsHistory": {},
        "sessionFeedback": {},
        "athleteWeeklySessions": {},
    }
    calibrations, history = build_calibrations(base_dt)
    seed["calibrations"] = calibrations
    seed["calibrationHistory"] = history

    for machine_index, machine in enumerate(MACHINES, start=1):
        seed["devices"][f"94-A9-90-11-9A-B{machine_index}"] = {
            "deviceId": f"94-A9-90-11-9A-B{machine_index}",
            "macAddress": f"94:A9:90:11:9A:B{machine_index}",
            "ipAddress": f"192.168.1.{80 + machine_index}",
            "machineId": machine.machine_id,
            "machineTypeId": machine.machine_type_id,
            "machineDisplayName": machine.display_name,
            "appState": "Idle",
            "activeUserUid": "",
            "encoderCalibrationValid": machine_index <= 2,
            "encoderZeroRaw": 1180 + machine_index * 14 if machine_index <= 2 else 0,
            "encoderFullRaw": 2840 + machine_index * 18 if machine_index <= 2 else 0,
            "encoderReferenceDistanceMm": 1000.0,
            "encoderInvertDirection": False,
            "wifiConnected": True,
            "updatedAtEpoch": epoch(base_dt),
            "updatedAtIso": iso_string(base_dt),
        }

    for user_index, user in enumerate(USERS):
        uid = user["uid"]
        seed["bodyMetricsHistory"][uid] = body_metrics_for_user(user, start_day, weeks)
        weekly_sessions = {}
        session_feedback = {}
        session_counter = 1

        for week in range(weeks):
            week_start = start_day + timedelta(days=week * 7)
            training_days = sorted(RNG.sample(range(0, 7), k=RNG.randint(4, 6)))
            week_key_value = week_key(week_start)
            week_node = {"days": {}}

            week_timelines = []
            for day_offset in training_days:
                current_day = week_start + timedelta(days=day_offset)
                day_timelines = []
                day_sessions = {}
                timeline_node = {}
                day_machine_count = RNG.randint(4, 5)
                machines_today = RNG.sample(MACHINES, k=day_machine_count)
                current_time = datetime.combine(current_day, time(6, 10), TZ)

                for machine in machines_today:
                    start_dt = current_time + timedelta(minutes=RNG.randint(0, 18))
                    calibration = calibrations[uid][machine.machine_type_id]
                    session_id, session, timeline = build_session(user, machine, calibration, start_dt, session_counter)
                    session_counter += 1
                    day_sessions[session_id] = session
                    timeline_key = f"{timeline['ordering']['startedAtEpoch']}_{session_id}"
                    timeline_node[timeline_key] = timeline
                    day_timelines.append(timeline)
                    week_timelines.append(timeline)

                    session_feedback[session_id] = {
                        "sessionId": session_id,
                        "rpe": RNG.randint(6, 9),
                        "energyScore": RNG.randint(3, 5),
                        "sorenessScore": RNG.randint(1, 4),
                        "painScore": RNG.randint(0, 2),
                        "readinessScore": RNG.randint(3, 5),
                        "notes": RNG.choice(
                            [
                                "Strong session with controlled tempo.",
                                "Good ROM overall, last machine felt heavy.",
                                "Energy improved after warm-up.",
                                "Felt consistent across all machine work.",
                                "Great pump, slight fatigue on final sets.",
                            ]
                        ),
                        "updatedAtEpoch": timeline["ordering"]["endedAtEpoch"] + 90,
                        "updatedAtIso": iso_string(datetime.fromtimestamp(timeline["ordering"]["endedAtEpoch"] + 90, TZ)),
                    }

                    current_time = datetime.fromtimestamp(timeline["ordering"]["endedAtEpoch"], TZ) + timedelta(minutes=18)

                last_timeline = sorted(day_timelines, key=lambda x: x["ordering"]["startedAtEpoch"])[-1]
                week_node["days"][day_key(current_day)] = {
                    "meta": {
                        "dayKey": day_key(current_day),
                        "weekKey": week_key_value,
                        "userUid": uid,
                        "userDisplayName": user["name"],
                        "lastSessionId": last_timeline["sessionId"],
                        "lastStartedAtEpoch": last_timeline["ordering"]["startedAtEpoch"],
                        "lastEndedAtEpoch": last_timeline["ordering"]["endedAtEpoch"],
                        "lastStartedAtIso": last_timeline["ordering"]["startedAtIso"],
                        "lastEndedAtIso": last_timeline["ordering"]["endedAtIso"],
                        "lastMachine": last_timeline["machine"],
                    },
                    "daySummary": aggregate_summary(day_timelines, "day"),
                    "timeline": dict(sorted(timeline_node.items())),
                    "sessions": day_sessions,
                }

            week_node["weekSummary"] = aggregate_summary(week_timelines, "week")
            weekly_sessions[week_key_value] = week_node

        seed["athleteWeeklySessions"][uid] = weekly_sessions
        seed["sessionFeedback"][uid] = session_feedback

    return seed


def main() -> None:
    output_path = Path("sample_data/firebase_rtdb_dashboard_seed_2026_w12_w17.json")
    data = build_seed()
    output_path.write_text(json.dumps(data, indent=2), encoding="utf-8")
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
