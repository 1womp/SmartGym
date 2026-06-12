import { useEffect, useState } from "react";
import { db } from "../firebase";
import { ref, get, set } from "firebase/database";

const ROUTINE = [
  {
    id: "leg_press",
    machine: "Leg Press",
    target: "Piernas / glúteos",
    sets: "3 sets",
    reps: "10 reps",
    note: "Mantén control en la fase excéntrica.",
  },
  {
    id: "leg_extension",
    machine: "Leg Extension",
    target: "Cuádriceps",
    sets: "3 sets",
    reps: "12 reps",
    note: "Evita movimientos bruscos al extender.",
  },
  {
    id: "seated_leg_curl",
    machine: "Seated Leg Curl",
    target: "Isquiotibiales",
    sets: "3 sets",
    reps: "10 reps",
    note: "Pausa breve al final del recorrido.",
  },
  {
    id: "lat_pulldown",
    machine: "Lat Pulldown",
    target: "Espalda",
    sets: "3 sets",
    reps: "10 reps",
    note: "Lleva la barra al pecho con control.",
  },
  {
    id: "chest_press",
    machine: "Chest Press",
    target: "Pecho / tríceps",
    sets: "3 sets",
    reps: "10 reps",
    note: "No bloquees los codos al final.",
  },
];

const getTodayKey = () => {
  const now = new Date();

  return now.toLocaleDateString("en-CA", {
    timeZone: "America/Monterrey",
  });
};

export default function RecommendedRoutine({ rfid }) {
  const [checked, setChecked] = useState({});
  const [selectedDate, setSelectedDate] = useState(getTodayKey());
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    if (!rfid || !selectedDate) return;

    loadProgress();
  }, [rfid, selectedDate]);

  const loadProgress = async () => {
    setLoading(true);

    const snap = await get(
      ref(db, `recommendedRoutineProgress/${rfid}/${selectedDate}`)
    );

    if (snap.exists()) {
      setChecked(snap.val());
    } else {
      setChecked({});
    }

    setLoading(false);
  };

  const saveProgress = async (nextChecked) => {
    await set(
      ref(db, `recommendedRoutineProgress/${rfid}/${selectedDate}`),
      {
        ...nextChecked,
        updatedAt: new Date().toISOString(),
      }
    );
  };

  const toggleExercise = async (id) => {
    const nextChecked = {
      ...checked,
      [id]: !checked[id],
    };

    setChecked(nextChecked);
    await saveProgress(nextChecked);
  };

  const completed = ROUTINE.filter((exercise) => checked[exercise.id]).length;

  return (
    <div className="calendar-card">
      <h2>Recommended Routine</h2>

      <p className="routine-date">
        Rutina sugerida para una sesión completa de gimnasio.
      </p>

      <div className="routine-date-selector">
        <label>Date:</label>

        <input
          type="date"
          value={selectedDate}
          onChange={(e) => setSelectedDate(e.target.value)}
        />
      </div>

      <div className="routine-progress">
        Completed: <strong>{completed}</strong> / {ROUTINE.length}
      </div>

      {loading ? (
        <p>Loading routine...</p>
      ) : (
        <div className="routine-checklist">
          {ROUTINE.map((exercise) => (
            <label
              key={exercise.id}
              className={`routine-check-item ${
                checked[exercise.id] ? "completed" : ""
              }`}
            >
              <input
                type="checkbox"
                checked={!!checked[exercise.id]}
                onChange={() => toggleExercise(exercise.id)}
              />

              <div>
                <strong>{exercise.machine}</strong>
                <p>{exercise.target}</p>
                <span>
                  {exercise.sets} · {exercise.reps}
                </span>
                <small>{exercise.note}</small>
              </div>
            </label>
          ))}
        </div>
      )}
    </div>
  );
}