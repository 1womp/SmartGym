import { PieChart, Pie, Cell, Tooltip } from "recharts";

const RADIAN = Math.PI / 180;

const renderLabel = ({ cx, cy, midAngle, innerRadius, outerRadius, value }) => {
  if (!value) return null;

  const radius = innerRadius + (outerRadius - innerRadius) * 0.55;
  const x = cx + radius * Math.cos(-midAngle * RADIAN);
  const y = cy + radius * Math.sin(-midAngle * RADIAN);

  return (
    <text
      x={x}
      y={y}
      fill="white"
      textAnchor="middle"
      dominantBaseline="central"
      fontWeight="bold"
      fontSize={15}
    >
      {value}
    </text>
  );
};

export default function FormPieChart({
  valid,
  invalid,
  selectedWeek,
  weeks = [],
  onWeekChange,
}) {
  const data = [
    { name: "Valid", value: valid },
    { name: "Invalid", value: invalid },
  ];

  const COLORS = ["#22c55e", "#ef4444"];

  return (
    <div className="pie-card">
      <div className="pie-header">
        <h3>Repetitions per Week</h3>

        <select
          className="pie-week-select"
          value={selectedWeek}
          onChange={(e) => onWeekChange(e.target.value)}
        >
          {weeks.map((week) => (
            <option key={week} value={week}>
              {week}
            </option>
          ))}
        </select>
      </div>

      {valid === 0 && invalid === 0 ? (
        <p>No hay datos</p>
      ) : (
        <>
          <div className="pie-chart-center">
            <PieChart width={260} height={220}>
              <Pie
                data={data.filter((item) => item.value > 0)}
                dataKey="value"
                outerRadius={75}
                label={renderLabel}
                labelLine={false}
              >
                {data.map((_, i) => (
                  <Cell key={i} fill={COLORS[i]} />
                ))}
              </Pie>

              <Tooltip
                formatter={(v, name) => [`${v} reps`, name]}
                contentStyle={{
                  background: "#0f172a",
                  border: "none",
                  borderRadius: 8,
                }}
              />
            </PieChart>
          </div>

          <div className="pie-legend">
            <p>
              <span style={{ color: "#22c55e" }}>●</span>{" "}
              Valid: <strong>{valid}</strong>
            </p>

            <p>
              <span style={{ color: "#ef4444" }}>●</span>{" "}
              Invalid: <strong>{invalid}</strong>
            </p>
          </div>
          <div className="repetition-summary">
            <p>
              Total repetitions: <strong>{valid + invalid}</strong>
            </p>
          </div>
        </>
      )}
    </div>
  );
}