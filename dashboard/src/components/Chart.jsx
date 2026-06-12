import { useEffect, useState } from "react";
import { db } from "../firebase";
import { ref, get } from "firebase/database";
import VelocityChart from "./VelocityChart";
import WeightChart from "./WeightChart";
import FormPieChart from "./FormPieChart";
import { buildWeeklyData } from "../utils/chartHelpers";

export default function Chart({
  rfid,
  machine,
  latestWeek,
  compareWeek,
  onOpenWeights,
  machineLabel,
}) {
  const [latestVelocity, setLatestVelocity] = useState([]);
  const [compareVelocity, setCompareVelocity] = useState([]);
  const [latestWeight, setLatestWeight] = useState([]);
  const [compareWeight, setCompareWeight] = useState([]);

  const [valid, setValid] = useState(0);
  const [invalid, setInvalid] = useState(0);

  const [velocityImp, setVelocityImp] = useState(null);
  const [weightImp, setWeightImp] = useState(null);

  const [repetitionWeeks, setRepetitionWeeks] = useState([]);
  const [selectedRepetitionWeek, setSelectedRepetitionWeek] = useState("");

  useEffect(() => {
    if (!rfid || !machine || !latestWeek) return;

    loadCharts();
    loadRepetitionWeeks();
  }, [rfid, machine, latestWeek, compareWeek]);

  useEffect(() => {
    if (!rfid || !machine || !selectedRepetitionWeek) return;

    loadRepetitionData(selectedRepetitionWeek);
  }, [selectedRepetitionWeek]);

  const calcImprovement = (compareArr, latestArr) => {
    const avg = (arr) => {
      const values = arr.filter((d) => d.value > 0);
      if (!values.length) return 0;

      return values.reduce((a, d) => a + d.value, 0) / values.length;
    };

    const cAvg = avg(compareArr);
    const lAvg = avg(latestArr);

    if (cAvg === 0) return null;

    return (((lAvg - cAvg) / cAvg) * 100).toFixed(1);
  };

  const weekSortDesc = (a, b) => {
    const [yearA, weekA] = a.split("-W").map(Number);
    const [yearB, weekB] = b.split("-W").map(Number);

    if (yearA !== yearB) return yearB - yearA;
    return weekB - weekA;
  };

  const weekHasMachineData = (weekData) => {
    const days = weekData?.days || {};

    return Object.values(days).some((day) =>
      Object.values(day?.sessions || {}).some((session) => {
        const machineType =
          session?.machineTypeId ||
          session?.machine?.machineTypeId;

        return machineType === machine && session?.summary;
      })
    );
  };

  const loadRepetitionWeeks = async () => {
    const snap = await get(ref(db, `athleteWeeklySessions/${rfid}`));

    if (!snap.exists()) {
      setRepetitionWeeks([]);
      setSelectedRepetitionWeek("");
      return;
    }

    const allWeeks = snap.val();

    const weeksWithMachineData = Object.entries(allWeeks || {})
      .filter(([, weekData]) => weekHasMachineData(weekData))
      .map(([week]) => week)
      .sort(weekSortDesc);

    setRepetitionWeeks(weeksWithMachineData);

    if (
      weeksWithMachineData.length > 0 &&
      !weeksWithMachineData.includes(selectedRepetitionWeek)
    ) {
      setSelectedRepetitionWeek(latestWeek || weeksWithMachineData[0]);
    }
  };

  const loadWeekData = async (week) => {
    const snap = await get(
      ref(db, `athleteWeeklySessions/${rfid}/${week}/days`)
    );

    if (!snap.exists()) return null;

    return buildWeeklyData(snap.val(), machine);
  };

  const loadCharts = async () => {
    const latestData = await loadWeekData(latestWeek);

    if (!latestData) {
      setLatestVelocity([]);
      setLatestWeight([]);
      return;
    }

    setLatestVelocity(latestData.velocityData);
    setLatestWeight(latestData.weightData);

    if (compareWeek && compareWeek !== latestWeek) {
      const compareData = await loadWeekData(compareWeek);

      if (compareData) {
        setCompareVelocity(compareData.velocityData);
        setCompareWeight(compareData.weightData);

        setVelocityImp(
          calcImprovement(compareData.velocityData, latestData.velocityData)
        );

        setWeightImp(
          calcImprovement(compareData.weightData, latestData.weightData)
        );

        return;
      }
    }

    setCompareVelocity([]);
    setCompareWeight([]);
    setVelocityImp(null);
    setWeightImp(null);
  };

  const loadRepetitionData = async (week) => {
    const data = await loadWeekData(week);

    if (!data) {
      setValid(0);
      setInvalid(0);
      return;
    }

    setValid(data.valid);
    setInvalid(data.invalid);
  };

  return (
    <div className="chart-container">
      <div className="charts-two-col">
        <VelocityChart
          latestData={latestVelocity}
          compareData={compareVelocity}
          improvement={velocityImp}
          latestWeek={latestWeek}
        />

        <WeightChart
          latestData={latestWeight}
          compareData={compareWeight}
          improvement={weightImp}
          latestWeek={latestWeek}
        />
      </div>

      <div className="dashboard-two-col">
        <FormPieChart
          valid={valid}
          invalid={invalid}
          selectedWeek={selectedRepetitionWeek}
          weeks={repetitionWeeks}
          onWeekChange={setSelectedRepetitionWeek}
        />

        <div
          className="routine-card"
          onClick={onOpenWeights}
          style={{ cursor: "pointer" }}
        >
          <h3>📅 Weight Record</h3>

          <p className="routine-date">
            Ver pesos seleccionados por día del mes para la máquina actual.
          </p>

          <p className="routine-placeholder">
            Máquina seleccionada: {machineLabel}
          </p>
        </div>
      </div>
    </div>
  );
}