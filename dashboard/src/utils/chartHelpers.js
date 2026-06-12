export const buildWeeklyData = (days, machine) => {
  const order = ["Lun", "Mar", "Mié", "Jue", "Vie", "Sáb", "Dom"];

  const resultByDay = {
    Lun: { velocityValues: [], volume: 0, valid: 0, invalid: 0 },
    Mar: { velocityValues: [], volume: 0, valid: 0, invalid: 0 },
    Mié: { velocityValues: [], volume: 0, valid: 0, invalid: 0 },
    Jue: { velocityValues: [], volume: 0, valid: 0, invalid: 0 },
    Vie: { velocityValues: [], volume: 0, valid: 0, invalid: 0 },
    Sáb: { velocityValues: [], volume: 0, valid: 0, invalid: 0 },
    Dom: { velocityValues: [], volume: 0, valid: 0, invalid: 0 },
  };

  Object.entries(days || {}).forEach(([date, day]) => {
    const jsDate = new Date(`${date}T00:00:00`);
    const dayLabel = order[jsDate.getDay() === 0 ? 6 : jsDate.getDay() - 1];

    Object.values(day?.sessions || {}).forEach((session) => {
      const machineType =
        session?.machineTypeId ||
        session?.machine?.machineTypeId;

      if (machineType !== machine) return;

      const summary = session?.summary || {};

      const velocity = Number(summary.avgPeakVelocityPctPerSec ?? 0);
      const volume = Number(summary.volumeLoadKg ?? 0);

      if (velocity > 0) {
        resultByDay[dayLabel].velocityValues.push(velocity);
      }

      resultByDay[dayLabel].volume += volume;
      resultByDay[dayLabel].valid += Number(summary.validReps ?? 0);
      resultByDay[dayLabel].invalid += Number(summary.invalidReps ?? 0);
    });
  });

  return {
    velocityData: order.map((day) => {
      const values = resultByDay[day].velocityValues;

      return {
        day,
        value: values.length
          ? Number((values.reduce((a, b) => a + b, 0) / values.length).toFixed(2))
          : 0,
      };
    }),

    weightData: order.map((day) => ({
      day,
      value: Number(resultByDay[day].volume.toFixed(0)),
    })),

    valid: order.reduce((sum, day) => sum + resultByDay[day].valid, 0),
    invalid: order.reduce((sum, day) => sum + resultByDay[day].invalid, 0),
  };
};