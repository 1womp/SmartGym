// components/WeightChart.jsx — REEMPLAZAR TODO EL ARCHIVO

import {
  LineChart, Line, XAxis, YAxis,
  Tooltip, Legend, ResponsiveContainer,
} from "recharts";

export default function WeightChart({
  latestData = [],
  compareData = [],
  improvement,
  latestWeek,
}) {

  const latestLabel = latestWeek
    ? `Last Session (${latestWeek})`
    : "Last Session";

  const merged = latestData.map((d, i) => ({
    day: d.day,
    [latestLabel]: d.value,
    "Compared Week": compareData?.[i]?.value ?? null,
  }));

  const hasCompare = compareData.length > 0;

  return (
    <div className="chart-card">
      <div className="chart-top">
        <h3>Total Volume</h3>

        {improvement !== null && (
          <span className={`improvement ${Number(improvement) >= 0 ? "positive" : "negative"}`}>
            {Number(improvement) >= 0 ? "+" : ""}
            {improvement}%
          </span>
        )}
      </div>

      <ResponsiveContainer width="100%" height={250}>
        <LineChart data={merged}>
          <XAxis dataKey="day" stroke="#94a3b8" />
          <YAxis stroke="#94a3b8" />
          <Tooltip
            contentStyle={{ background: "#0f172a", border: "none", borderRadius: 8 }}
            labelStyle={{ color: "#94a3b8" }}
          />
          <Legend />
          <Line
            type="monotone"
            dataKey={latestLabel}
            stroke="#3b82f6"
            strokeWidth={3}
            dot={{ r: 4 }}
            connectNulls
          />
          {hasCompare && (
            <Line
              type="monotone"
              dataKey="Compared Week"
              stroke="#22c55e"
              strokeWidth={2}
              strokeDasharray="5 5"
              dot={{ r: 3 }}
              connectNulls
            />
          )}
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}