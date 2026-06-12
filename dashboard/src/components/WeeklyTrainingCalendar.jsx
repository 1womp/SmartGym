import { useEffect, useState } from "react";
import { db } from "../firebase";
import { ref, get } from "firebase/database";

const MACHINE_NAMES = {
  chest_press: "Chest Press",
  lat_pull: "Lat Pull",
  leg_ext: "Leg Extension",
  seated_row: "Seated Row",
  seated_leg_curl: "seated Leg Curl",
  hip_abductor:"Hip Abductor",
  flat_bench_press: "Bench Press",
  incline_cable_curl: "Incline Curl",
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

const DAY_NAMES = [
  "Domingo",
  "Lunes",
  "Martes",
  "Miércoles",
  "Jueves",
  "Viernes",
  "Sábado",
];

export default function WeeklyTrainingCalendar({ rfid, machines = [], machineLabels = {} }) {
  const [weeks, setWeeks] = useState([]);
  const [selectedWeek, setSelectedWeek] = useState("");
  const [weekData, setWeekData] = useState({});
  const [expanded, setExpanded] = useState({});
  const [machineFilter, setMachineFilter] = useState("all");

  useEffect(() => {
    if (rfid) {
      loadWeeks();
    }
  }, [rfid]);

  useEffect(() => {
    if (selectedWeek) {
      loadWeekData();
    }
  }, [selectedWeek]);

  const loadWeeks = async () => {
    const snap = await get(
      ref(db, `athleteWeeklySessions/${rfid}`)
    );

    if (!snap.exists()) return;

    const weekList = Object.keys(
      snap.val()
    ).sort((a, b) => {
      const [yA, wA] = a.split("-W").map(Number);
      const [yB, wB] = b.split("-W").map(Number);

      if (yA !== yB) return yA - yB;

      return wA - wB;
    });

    setWeeks(weekList);

    setSelectedWeek(
      weekList[weekList.length - 1]
    );
  };

  const loadWeekData = async () => {
    const snap = await get(
      ref(
        db,
        `athleteWeeklySessions/${rfid}/${selectedWeek}/days`
      )
    );

    if (!snap.exists()) {
      setWeekData({});
      return;
    }

    setWeekData(snap.val());
  };

  const changeWeek = (direction) => {
    const currentIndex =
      weeks.indexOf(selectedWeek);

    const nextIndex =
      currentIndex + direction;

    if (
      nextIndex >= 0 &&
      nextIndex < weeks.length
    ) {
      setSelectedWeek(
        weeks[nextIndex]
      );
    }
  };

  const fullWeek = [];

  const weekDates =
    Object.keys(weekData).sort();

  if (weekDates.length > 0) {

    const firstDate =
      new Date(weekDates[0]);

    const startOfWeek =
      new Date(firstDate);

    startOfWeek.setDate(
      firstDate.getDate() -
      firstDate.getDay()
    );

    for (let i = 0; i < 7; i++) {

      const current =
        new Date(startOfWeek);

      current.setDate(
        startOfWeek.getDate() + i
      );

      const dateString =
        current
          .toISOString()
          .split("T")[0];

      fullWeek.push({
        date: dateString,
        data:
          weekData[dateString] ||
          null,
      });
    }
  }

  return (
    <div className="calendar-card">

      <div className="calendar-header">

        <h2>
          Workout Record
        </h2>

        <div className="calendar-nav">

          <button
            onClick={() =>
              changeWeek(-1)
            }
          >
            ←
          </button>

          <span>
            {selectedWeek}
          </span>

          <button
            onClick={() =>
              changeWeek(1)
            }
          >
            →
          </button>

        </div>

      </div>

      <div className="training-filter">
        <label>Machine:</label>

        <select
          value={machineFilter}
          onChange={(e) => setMachineFilter(e.target.value)}
        >
          <option value="all">All machines</option>

          {machines.map((m) => (
            <option key={m} value={m}>
              {machineLabels[m] || m}
            </option>
          ))}
        </select>
      </div>

      <div className="week-calendar">

        {fullWeek.map(
          ({ date, data }) => {

            const groupedMachines =
              {};

            if (data) {

              Object.values(
                data.sessions || {}
              ).forEach(
                (session) => {

                  const machine =
                    session?.machineTypeId ||
                    session?.machine?.machineTypeId;

                  if (!machine) return;

                  if (
                    machineFilter !== "all" &&
                    machine !== machineFilter
                  ) {
                    return;
                  }

                  if (
                    !groupedMachines[
                      machine
                    ]
                  ) {
                    groupedMachines[
                      machine
                    ] = [];
                  }

                  groupedMachines[
                    machine
                  ].push(session);
                }
              );
            }

            return (
              <div
                key={date}
                className="week-day"
              >

                <div className="week-date">

                  <strong>
                    {
                      DAY_NAMES[
                        new Date(
                          date
                        ).getDay()
                      ]
                    }
                  </strong>

                  <br />

                  {date}

                </div>

                {!data && (
                  <div className="empty-day">
                    Sin entrenamientos
                  </div>
                )}

                {Object.entries(
                  groupedMachines
                ).map(
                  ([
                    machine,
                    sessions,
                  ]) => {

                    const machineKey =
                      `${date}-${machine}`;

                    return (
                      <div
                        key={
                          machineKey
                        }
                        className="training-session"
                      >

                        <div
                          className={`machine-header ${machine}`}
                          onClick={() =>
                            setExpanded(
                              (
                                prev
                              ) => ({
                                ...prev,
                                [machineKey]: !prev[machineKey],
                              })
                            )
                          }
                        >

                          <span>
                            {expanded[
                              machineKey
                            ]
                              ? "▼"
                              : "▶"}
                          </span>

                          <span className={`machine-badge ${machine}`}>
                            {MACHINE_NAMES[machine] || machine}
                          </span>

                          <span
                            style={{
                              marginLeft:
                                "auto",
                              opacity:
                                0.7,
                            }}
                          >
                          </span>

                        </div>

                        {expanded[
                          machineKey
                        ] && (

                          <div className="session-details">

                            {sessions.map(
                              (
                                session,
                                index
                              ) => {

                                const weight =
                                  session
                                    ?.plan
                                    ?.selectedWeightKg ??
                                  0;

                                const valid =
                                  session
                                    ?.summary
                                    ?.validReps ??
                                  0;

                                const invalid =
                                  session
                                    ?.summary
                                    ?.invalidReps ??
                                  0;

                                const volume =
                                  session
                                    ?.summary
                                    ?.volumeLoadKg ??
                                  0;

                                const velocity =
                                  session
                                    ?.summary
                                    ?.avgPeakVelocityPctPerSec ??
                                  0;

                                return (
                                  <div
                                    key={
                                      index
                                    }
                                    className="session-card"
                                  >

                                    <div className="session-metric">
                                      <span>
                                        Peso
                                      </span>
                                      <strong>
                                        {
                                          weight
                                        } kg
                                      </strong>
                                    </div>

                                    <div className="session-metric">
                                      <span>
                                        Válidas
                                      </span>
                                      <strong>
                                        {
                                          valid
                                        }
                                      </strong>
                                    </div>

                                    <div className="session-metric">
                                      <span>
                                        Inválidas
                                      </span>
                                      <strong>
                                        {
                                          invalid
                                        }
                                      </strong>
                                    </div>

                                    <div className="session-metric">
                                      <span>
                                        Volumen
                                      </span>
                                      <strong>
                                        {volume.toFixed(
                                          0
                                        )} kg
                                      </strong>
                                    </div>

                                    <div className="session-metric">
                                      <span>
                                        Velocidad
                                      </span>
                                      <strong>
                                        {Number(
                                          velocity
                                        ).toFixed(
                                          1
                                        )}
                                      </strong>
                                    </div>

                                  </div>
                                );
                              }
                            )}

                          </div>

                        )}

                      </div>
                    );
                  }
                )}

              </div>
            );
          }
        )}

      </div>

    </div>
  );
}