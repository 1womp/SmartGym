export const filterWeeksByMachine = (
  allData,
  machine
) => {
  const validWeeks = [];

  Object.entries(allData || {}).forEach(
    ([week, weekData]) => {
      const days = weekData?.days || {};

      let found = false;

      Object.values(days).forEach((day) => {
        const sessions =
          day?.sessions || {};

        Object.values(sessions).forEach(
          (session) => {
            const machineType =
              session?.identity?.machine
                ?.machineTypeId;

            if (machineType === machine) {
              found = true;
            }
          }
        );
      });

      if (found) {
         validWeeks.push(week);
      }
    }
  );

  return validWeeks;
};