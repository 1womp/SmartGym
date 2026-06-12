# SmartGym Dashboard

This folder contains the teammate React/Vite dashboard for visualizing SmartGym Firebase data.

## Stack

- React
- Vite
- Firebase Web SDK
- Recharts

## Setup

```powershell
cd dashboard
npm install
npm run dev
```

For production build:

```powershell
npm run build
npm run preview
```

## Firebase Connection

The dashboard initializes Firebase in `src/firebase.js` from Vite environment variables. Copy `.env.example` to `.env` locally and fill in the real Firebase web config. Do not commit `.env`.

Current known paths used by the dashboard include:

- `usersByRfid/{rfid}` for user profile data.
- `athleteWeeklySessions/{rfid}` for weekly/day/session history.
- `athleteWeeklySessions/{rfid}/{week}/days` for calendar and charts.
- `recommendedRoutineProgress/{rfid}/{selectedDate}` for routine progress UI.

The firmware writes workout data under `athleteWeeklySessions`, including day summaries, week summaries, timelines, session roots, set details, representative reps, and rep sets.

## Notes

- `node_modules/` is intentionally excluded from the repository.
- Run `npm install` after cloning.
- Firebase paths should stay compatible with the firmware schema.
- Real Firebase keys and project IDs should stay in local environment files or deployment secrets.

## Screenshots

Add screenshots to `../screenshots/` and reference them here when available.
