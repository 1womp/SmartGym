import { useState, useEffect } from "react";
import Chart from "./components/Chart";
import { db } from "./firebase";
import { ref, get, update } from "firebase/database";
import MonthlyWeightCalendar from "./components/MonthlyWeightCalendar";
//import TrainingHistoryWeek from "./components/NOOOTrainingHistoryWeek";
import WeeklyTrainingCalendar from "./components/WeeklyTrainingCalendar";
import { buildWeeklyData } from "./utils/chartHelpers";
import RecommendedRoutine from "./components/RecommendedRoutine";

const MACHINE_LABELS = {
  chest_press: "Chest Press",
  lat_pull: "Lat Pull",
  leg_ext: "Leg Extension",
  seated_row: "Seated Row",
  seated_leg_curl: "seated Leg Curl",
  hip_abductor:"Hip Abductor",
  flat_bench_press: "Bench Press",
  incline_cable_curl: "Incline Cable Curl",
  preacher_curl: "Preacher Curl",
  cable_fly: "Cable Fly",
  shoulder_press: "Shoulder Press",
  incline_press:"Incline Press",
  triceps_pushdown: "Triceps Pushdown",
  overhead_triceps_extension: "OH Triceps",
  cable_lateral_raise: "Cable Lateral Raise",
  seated_cable_row: "Cable Row",
  leg_press: "Leg Press",
  leg_extension: "Leg Extension",
  calf_raise:"Calf Raise",
  hip_adductor: "Hip Adductor",
  lat_pulldown: "Lat Pulldown",
};

function App() {
  const [rfid, setRfid] = useState("");
  const [user, setUser] = useState(null);
  const [view, setView] = useState("dashboard");

  const [latestWeek,  setLatestWeek]  = useState("");
  const [compareWeek, setCompareWeek] = useState("");
  const [weeks,       setWeeks]       = useState([]);
  const [latestData, setLatestData] = useState([]);

  const [machines, setMachines] = useState([]);
  const [machine,  setMachine]  = useState("");

  const [machineStats,  setMachineStats]  = useState(null);
  const [achievements,  setAchievements]  = useState({ maxReps: 0, maxWeight: 0, activeDays: 0 });

  const [machineLatestWeek, setMachineLatestWeek] = useState("");
  const [machineWeeks, setMachineWeeks] = useState([]);


  const [editData, setEditData] = useState({
    displayName: "", age: "", weightKg: "", heightCm: "", goal: "", gender: "",
  });

  // Recarga stats de mÃ¡quina al cambiar ejercicio o semana
  useEffect(() => {
    const refreshMachineData = async () => {
      if (!rfid || !machine) return;

      const realLatestWeek = await getLatestWeekForMachine(rfid, machine);
      setMachineLatestWeek(realLatestWeek);

      const validMachineWeeks = await getWeeksForMachine(rfid, machine);
      setMachineWeeks(validMachineWeeks);

      const availableCompareWeeks = validMachineWeeks.filter(
        (w) => w !== realLatestWeek
      );

      if (!availableCompareWeeks.includes(compareWeek)) {
        setCompareWeek(availableCompareWeeks.at(-1) || "");
      }

      // Esto actualiza Max Weight y Max Reps segÃºn la mÃ¡quina seleccionada
      await loadAchievements(rfid, machine);

      if (realLatestWeek) {
        await loadMachineStats(rfid, realLatestWeek, machine);

        if (compareWeek === realLatestWeek) {
          const nextCompare = validMachineWeeks
            .filter((w) => w !== realLatestWeek)
            .at(-1);

          setCompareWeek(nextCompare || "");
        }
      }
    };

    refreshMachineData();
  }, [rfid, machine, weeks]);

  // â”€â”€â”€ HELPERS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  const sortWeeks = (arr) =>
    [...arr].sort((a, b) => {
      const [yA, wA] = a.split("-W").map(Number);
      const [yB, wB] = b.split("-W").map(Number);
      return yA !== yB ? yA - yB : wA - wB;
    });

  // â”€â”€â”€ CARGA SEMANAS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  const getLatestWeek = async (rfid) => {
    const snap = await get(ref(db, `athleteWeeklySessions/${rfid}`));
    if (!snap.exists()) return null;

    const allWeeks = sortWeeks(Object.keys(snap.val()));
    setWeeks(allWeeks);

    const latest = allWeeks[allWeeks.length - 1];
    setLatestWeek(latest);

    // Semana de comparaciÃ³n: la anterior a la Ãºltima (si existe)
    const compareOptions = allWeeks.slice(0, -1);
    if (compareOptions.length > 0) {
      setCompareWeek(compareOptions[compareOptions.length - 1]);
    }

    return latest;
  };

  // â”€â”€â”€ CARGA MÃQUINAS (de la semana mÃ¡s reciente) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  const loadMachines = async (rfid) => {
    const snap = await get(
      ref(db, `athleteWeeklySessions/${rfid}`)
    );

    if (!snap.exists()) return;

    const machineSet = new Set();
    const allWeeks = snap.val();

    Object.values(allWeeks || {}).forEach((weekData) => {
      Object.values(weekData?.days || {}).forEach((day) => {
        Object.values(day?.sessions || {}).forEach((session) => {
          const machineType =
            session?.machineTypeId ||
            session?.machine?.machineTypeId;

          if (machineType && typeof machineType === "string") {
            machineSet.add(machineType.trim());
          }
        });
      });
    });

    const arr = [...machineSet].sort();

    setMachines(arr);

    if (arr.length > 0 && !machine) {
      setMachine(arr[0]);
    }
  };

  const getLatestWeekForMachine = async (rfid, machine) => {
    const snap = await get(ref(db, `athleteWeeklySessions/${rfid}`));

    if (!snap.exists()) return "";

    const allWeeks = snap.val();

    const sortedWeeks = Object.keys(allWeeks).sort((a, b) => {
      const [yearA, weekA] = a.split("-W").map(Number);
      const [yearB, weekB] = b.split("-W").map(Number);

      if (yearA !== yearB) return yearB - yearA;
      return weekB - weekA;
    });

    for (const week of sortedWeeks) {
      const days = allWeeks[week]?.days || {};

      const hasMachineData = Object.values(days).some((day) =>
        Object.values(day?.sessions || {}).some((session) => {
          const machineType =
            session?.machineTypeId ||
            session?.machine?.machineTypeId;

          return machineType === machine && session?.summary;
        })
      );

      if (hasMachineData) return week;
    }

    return "";
  };

  const getWeeksForMachine = async (rfid, machine) => {
    const snap = await get(ref(db, `athleteWeeklySessions/${rfid}`));

    if (!snap.exists()) return [];

    const allWeeks = snap.val();

    const weeksWithData = Object.entries(allWeeks || {})
      .filter(([, weekData]) => {
        const days = weekData?.days || {};

        return Object.values(days).some((day) =>
          Object.values(day?.sessions || {}).some((session) => {
            const machineType =
              session?.machineTypeId ||
              session?.machine?.machineTypeId;

            return machineType === machine && session?.summary;
          })
        );
      })
      .map(([week]) => week)
      .sort((a, b) => {
        const [yearA, weekA] = a.split("-W").map(Number);
        const [yearB, weekB] = b.split("-W").map(Number);

        if (yearA !== yearB) return yearA - yearB;
        return weekA - weekB;
      });

    return weeksWithData;
  };

  // â”€â”€â”€ STATS FILTRADAS POR MÃQUINA â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  const loadMachineStats = async (rfid, week, machine) => {
    const snap = await get(
      ref(db, `athleteWeeklySessions/${rfid}/${week}/days`)
    );

    if (!snap.exists()) return;

    const days = snap.val();

    const velocityValues = [];
    const romValues = [];
    const qualityValues = [];

    let totalVolume = 0;

    Object.values(days || {}).forEach((day) => {
      Object.values(day?.sessions || {}).forEach((session) => {
        const machineType =
          session?.machineTypeId ||
          session?.machine?.machineTypeId;

        if (machineType !== machine) return;

        const summary = session?.summary || {};

        const velocity = Number(summary.avgPeakVelocityPctPerSec ?? 0);
        const volume = Number(summary.volumeLoadKg ?? 0);
        const rom = Number(summary.avgRomPercent ?? 0);
        const quality = Number(summary.qualityScore ?? 0);

        if (velocity > 0) velocityValues.push(velocity);
        if (rom > 0) romValues.push(rom);
        if (quality > 0) qualityValues.push(quality);

        totalVolume += volume;
      });
    });

    const avg = (arr) =>
      arr.length
        ? Number((arr.reduce((a, b) => a + b, 0) / arr.length).toFixed(2))
        : 0;

    setMachineStats({
      avgVelocity: avg(velocityValues),
      totalVolume: Number(totalVolume.toFixed(0)),
      avgRomPercent: avg(romValues),
      avgQualityScore: avg(qualityValues),
    });
  };


  // â”€â”€â”€ LOGROS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  const loadAchievements = async (rfid, machine) => {
    const snap = await get(ref(db, `athleteWeeklySessions/${rfid}`));
    if (!snap.exists()) return;

    const allWeeks = snap.val();

    let maxWeight = 0;
    let maxRepsAtMaxWeight = 0;

    const activeDaySet = new Set();

    Object.values(allWeeks || {}).forEach((week) => {
      Object.entries(week?.days || {}).forEach(([date, day]) => {
        const sessions = Object.values(day?.sessions || {});

        if (sessions.length > 0) {
          activeDaySet.add(date);
        }

        sessions.forEach((session) => {
          const machineType =
            session?.machineTypeId ||
            session?.machine?.machineTypeId;

          if (machineType !== machine) return;

          const selectedWeightKg = Number(
            session?.selectedWeightKg ??
              session?.plan?.selectedWeightKg ??
              0
          );

          const reps = Number(
            session?.summary?.validReps ??
              session?.summary?.repCount ??
              session?.repCount ??
              0
          );

          if (selectedWeightKg > maxWeight) {
            maxWeight = selectedWeightKg;
            maxRepsAtMaxWeight = reps;
          }

          if (selectedWeightKg === maxWeight && reps > maxRepsAtMaxWeight) {
            maxRepsAtMaxWeight = reps;
          }
        });
      });
    });

    const activeDates = [...activeDaySet].sort();

    let activeDays = 0;

    if (activeDates.length > 0) {
      const activeLookup = new Set(activeDates);
      let currentDate = new Date(`${activeDates[activeDates.length - 1]}T00:00:00`);

      while (true) {
        const dateKey = currentDate.toISOString().slice(0, 10);

        if (!activeLookup.has(dateKey)) break;

        activeDays += 1;
        currentDate.setDate(currentDate.getDate() - 1);
      }
    }

    setAchievements({
      maxReps: maxRepsAtMaxWeight,
      maxWeight,
      activeDays,
    });
  };

  // â”€â”€â”€ LOGIN â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  const handleLogin = async () => {
    const snap = await get(ref(db, `usersByRfid/${rfid}`));
    if (!snap.exists()) { alert("Usuario no encontrado"); return; }

    const userData = snap.val();
    setUser(userData);
    setEditData({
      displayName: userData.displayName || "",
      age:         userData.age         || "",
      weightKg:    userData.weightKg    || "",
      heightCm:    userData.heightCm    || "",
      goal:        userData.goal        || "",
      gender:      userData.gender      || "",
    });

    loadAchievements(rfid, machine);
    const latest = await getLatestWeek(rfid);
    if (latest) await loadMachines(rfid);
  };

  // â”€â”€â”€ GUARDAR PERFIL â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  const saveProfile = async () => {
    try {
      await update(ref(db, `usersByRfid/${rfid}`), {
        displayName: editData.displayName,
        age:         Number(editData.age),
        weightKg:    Number(editData.weightKg),
        heightCm:    Number(editData.heightCm),
        goal:        editData.goal,
        gender:      editData.gender,
      });
      setUser({ ...user, ...editData });
      alert("Perfil actualizado âœ…");
      setView("dashboard");
    } catch {
      alert("Error al guardar");
    }
  };

  // â”€â”€â”€ PANTALLAS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  if (!user) {
    return (
      <div className="login-center">
        <h1>SmartGym</h1>
        <input
          placeholder="Ingresa tu RFID"
          value={rfid}
          onChange={(e) => setRfid(e.target.value)}
          onKeyDown={(e) => e.key === "Enter" && handleLogin()}
        />
        <button onClick={handleLogin}>Entrar</button>
      </div>
    );
  }

  if (view === "weights") {
    return (
      <div className="app">

        <button
          className="back-btn"
          onClick={() => setView("dashboard")}
        >
          â† Back
        </button>

        <MonthlyWeightCalendar
          rfid={rfid}
          machine={machine}
        />

      </div>
    );
  }

  if (view === "trainingHistory") {
    return (
      <div className="app">

        <button
          className="back-btn"
          onClick={() =>
            setView("dashboard")
          }
        >
          â† Back
        </button>

        <WeeklyTrainingCalendar
          rfid={rfid}
          machines={machines}
          machineLabels={MACHINE_LABELS}
        />

      </div>
    );
  }

  if (view === "recommendedRoutine") {
    return (
      <div className="app">
        <button
          className="back-btn"
          onClick={() => setView("dashboard")}
        >
          â† Back
        </button>

        <RecommendedRoutine rfid={rfid} />
      </div>
    );
  }

  if (view === "profile") {
    return (
      <div className="app">
        <button className="back-btn" onClick={() => setView("dashboard")}>
          â† Back
        </button>

        <div className="profile-card">
          <h2>Editar perfil</h2>

          <div className="profile-row">
            <span>Name:</span>
            <input
              value={editData.displayName}
              onChange={(e) =>
                setEditData({ ...editData, displayName: e.target.value })
              }
            />
          </div>

          <div className="profile-row">
            <span>Age:</span>
            <input
              type="number"
              value={editData.age}
              onChange={(e) =>
                setEditData({ ...editData, age: e.target.value })
              }
            />
          </div>

          <div className="profile-row">
            <span>Weight (kg):</span>
            <input
              type="number"
              value={editData.weightKg}
              onChange={(e) =>
                setEditData({ ...editData, weightKg: e.target.value })
              }
            />
          </div>

          <div className="profile-row">
            <span>Height (cm):</span>
            <input
              type="number"
              value={editData.heightCm}
              onChange={(e) =>
                setEditData({ ...editData, heightCm: e.target.value })
              }
            />
          </div>

          <div className="profile-row">
            <span>Goal:</span>
            <select
              value={editData.goal}
              onChange={(e) =>
                setEditData({ ...editData, goal: e.target.value })
              }
            >
              <option value="">Goal</option>
              <option value="endurance">Endurance</option>
              <option value="hypertrophy">Hypertrophy</option>
              <option value="strength">Strength</option>
            </select>
          </div>

          <div className="profile-row">
            <span>Gender:</span>
            <select
              value={editData.gender}
              onChange={(e) =>
                setEditData({ ...editData, gender: e.target.value })
              }
            >
              <option value="">Gender</option>
              <option value="male">Male</option>
              <option value="female">Female</option>
            </select>
          </div>

          <button onClick={saveProfile}>Save changes</button>
        </div>
      </div>
    );
  }

  // Opciones del selector de comparaciÃ³n (todo menos la Ãºltima semana)
  const compareOptions = machineWeeks.filter((w) => w !== machineLatestWeek);

  return (
    <div className="app">

      {/* â”€â”€ HEADER â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */}
      <div className="header">
        <div className="left">
          <h2 className="user-name clickable" onClick={() => setView("profile")}>
            {user.displayName}
          </h2>
          <p className="header-sub">
            {user.weightKg} kg Â· {user.heightCm} cm Â· {user.age} years
          </p>
        </div>

        <div className="center">
          <p className="user-goal">Goal: {user.goal}</p>
        </div>

        <div className="right">
          <label className="label-sm">Exercise</label>
          <select
            className="select"
            value={machine}
            onChange={(e) => setMachine(e.target.value)}
          >
            {machines.map((m) => (
              <option key={m} value={m}>
                {MACHINE_LABELS[m] || m}
              </option>
            ))}
          </select>
        </div>
      </div>

      {/* â”€â”€ LOGROS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */}
      <div className="achievements">
        <h2>Achievements</h2>
        <div className="cards">
          <div className="card">
            <p>Max Reps</p>
            <h3>{achievements.maxReps}</h3>
          </div>
          <div className="card">
            <p>Max Weight</p>
            <h3>{achievements.maxWeight} kg</h3>
          </div>
          <div className="card">
            <p>Active Days</p>
            <h3>{achievements.activeDays}</h3>
          </div>
        </div>
      </div>

      {/* â”€â”€ HISTORIAL + RUTINA â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */}
      <div className="dashboard-two-col">
        <div
          className="routine-card"
          onClick={() => setView("trainingHistory")}
          style={{ cursor: "pointer" }}
        >
          <h3>ðŸ‹ï¸ Workout Record</h3>
          <p className="routine-date">Ver todas las sesiones realizadas.</p>
          <p className="routine-placeholder">
            Consulta ejercicios, pesos, repeticiones y volumen por semana.
          </p>
        </div>

        <div
          className="routine-card"
          onClick={() => setView("recommendedRoutine")}
          style={{ cursor: "pointer" }}
        >
          <h3>ðŸ’¡ Recommended Routine</h3>
          <p className="routine-date">Basada en tu progreso reciente</p>
          <p className="routine-placeholder">
            La recomendaciÃ³n personalizada estarÃ¡ disponible prÃ³ximamente.
          </p>
        </div>
      </div>

      {/* â”€â”€ KPIs â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */}
      {machineStats && (
        <div className="kpi-section">
          <h2>KPIs</h2>

          <div className="cards">
            <div className="card">
              <p>Average Velocity</p>
              <h3>{machineStats.avgVelocity} %/s</h3>
            </div>

            <div className="card">
              <p>Total Volume</p>
              <h3>{machineStats.totalVolume} kg</h3>
            </div>

            <div className="card">
              <p>Average ROM Percent</p>
              <h3>{machineStats.avgRomPercent}%</h3>
            </div>

            <div className="card">
              <p>Quality Score</p>
              <h3>{machineStats.avgQualityScore}/100</h3>
            </div>
          </div>
        </div>
      )}


      {/* â”€â”€ SELECTOR SEMANA COMPARACIÃ“N â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */}
      <div className="week-selector">
        <span className="label-sm">
          Last Session Week: <strong>{machineLatestWeek}</strong>
        </span>

        {compareOptions.length > 0 && (
          <>
            <label>Compare to:</label>
            <select
              value={compareWeek}
              onChange={(e) => setCompareWeek(e.target.value)}
            >
              {compareOptions.map((w) => (
                <option key={w} value={w}>{w}</option>
              ))}
            </select>
          </>
        )}
      </div>

      {/* â”€â”€ GRÃFICAS + REPETICIONES + PESOS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */}
      {machine && machineLatestWeek && (
        <Chart
          rfid={rfid}
          machine={machine}
          latestWeek={machineLatestWeek}
          compareWeek={compareWeek}
          onOpenWeights={() => setView("weights")}
          machineLabel={MACHINE_LABELS[machine] || machine}
        />
      )}


    </div>
  );
}

export default App;