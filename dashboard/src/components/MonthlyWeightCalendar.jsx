import { useEffect, useState } from "react";
import { db } from "../firebase";
import { ref, get } from "firebase/database";

export default function MonthlyWeightCalendar({ rfid, machine }) {
  const [currentMonth, setCurrentMonth] = useState(new Date());
  const [weightsByDay, setWeightsByDay] = useState({});

  useEffect(() => {
    if (rfid && machine) {
      loadMonthData();
    }
  }, [rfid, machine, currentMonth]);

  const loadMonthData = async () => {
    const snap = await get(ref(db, `athleteWeeklySessions/${rfid}`));

    if (!snap.exists()) {
      setWeightsByDay({});
      return;
    }

    const allWeeks = snap.val();
    const monthData = {};

    Object.values(allWeeks || {}).forEach((week) => {
      Object.entries(week?.days || {}).forEach(([date, day]) => {
        const dateObj = new Date(`${date}T00:00:00`);

        if (
          dateObj.getMonth() !== currentMonth.getMonth() ||
          dateObj.getFullYear() !== currentMonth.getFullYear()
        ) {
          return;
        }

        const uniqueWeights = new Set();

        Object.values(day?.sessions || {}).forEach((session) => {
          const machineType =
            session?.machineTypeId ||
            session?.machine?.machineTypeId;

          if (machineType !== machine) return;

          const selectedWeightKg =
            session?.selectedWeightKg ??
            session?.plan?.selectedWeightKg ??
            session?.summary?.selectedWeightKg;

          if (
            selectedWeightKg !== undefined &&
            selectedWeightKg !== null &&
            selectedWeightKg !== ""
          ) {
            uniqueWeights.add(Number(selectedWeightKg));
          }
        });

        const weights = [...uniqueWeights].sort((a, b) => a - b);

        if (weights.length > 0) {
          monthData[date] = {
            weights,
            count: weights.length,
          };
        }
      });
    });

    setWeightsByDay(monthData);
  };

  const changeMonth = (direction) => {
    const newDate = new Date(currentMonth);
    newDate.setMonth(currentMonth.getMonth() + direction);
    setCurrentMonth(newDate);
  };

  const year = currentMonth.getFullYear();
  const month = currentMonth.getMonth();

  const firstDay = new Date(year, month, 1);

  let firstWeekDay = firstDay.getDay();
  firstWeekDay = firstWeekDay === 0 ? 6 : firstWeekDay - 1;

  const daysInMonth = new Date(year, month + 1, 0).getDate();

  const cells = [];

  for (let i = 0; i < firstWeekDay; i++) {
    cells.push(null);
  }

  for (let day = 1; day <= daysInMonth; day++) {
    cells.push(day);
  }

  const monthTitle = currentMonth.toLocaleDateString("es-MX", {
    month: "long",
    year: "numeric",
  });

  return (
    <div className="calendar-card">
      <div className="calendar-header">
        <h2>Selected Weights</h2>

        <div className="calendar-nav">
          <button onClick={() => changeMonth(-1)}>←</button>

          <span>{monthTitle}</span>

          <button onClick={() => changeMonth(1)}>→</button>
        </div>
      </div>

      <div className="calendar-grid">
        {["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"].map((d) => (
          <div key={d} className="calendar-weekday">
            {d}
          </div>
        ))}

        {cells.map((day, index) => {
          if (!day) {
            return <div key={index} className="calendar-empty" />;
          }

          const dateString = `${year}-${String(month + 1).padStart(
            2,
            "0"
          )}-${String(day).padStart(2, "0")}`;

          const info = weightsByDay[dateString];

          return (
            <div key={index} className="calendar-day">
              <div className="calendar-date">{day}</div>

              {info ? (
                <div className="calendar-weight-list">
                  {info.weights.map((weight) => (
                    <span key={weight} className="calendar-weight-pill">
                      {weight} kg
                    </span>
                  ))}
                </div>
              ) : (
                <div className="calendar-no-weight">—</div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}