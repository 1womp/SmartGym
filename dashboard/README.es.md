# Dashboard SmartGym

Idioma: [English](README.md) | Espanol

Esta carpeta contiene el dashboard React/Vite para visualizar datos SmartGym desde Firebase.

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

Build de produccion:

```powershell
npm run build
npm run preview
```

## Conexion Firebase

El dashboard inicializa Firebase en `src/firebase.js` usando variables de entorno Vite. Copia `.env.example` a `.env` localmente y llena la configuracion real. No commitees `.env`.

Rutas conocidas:

- `usersByRfid/{rfid}` para perfil.
- `athleteWeeklySessions/{rfid}` para historial.
- `athleteWeeklySessions/{rfid}/{week}/days` para calendario/graficas.
- `recommendedRoutineProgress/{rfid}/{selectedDate}` para progreso de rutina.

El firmware escribe datos bajo `athleteWeeklySessions`, incluyendo resumenes diarios, resumenes semanales, timelines, raiz de sesion, detalles de sets, reps representativas y rep sets.

Para datos demo/prueba, consulta la [Guia de seed Firebase para dashboard](../docs/FIREBASE_DASHBOARD_SEED_GUIDE.es.md). Explica el dataset generado y el flujo de importacion segura.

## Notas

- `node_modules/` no se incluye en el repo.
- Ejecuta `npm install` despues de clonar.
- Las rutas Firebase deben mantenerse compatibles con el firmware.
- Claves reales Firebase y project IDs deben quedarse en `.env` local o secretos de despliegue.

## Capturas

Agrega capturas en `../screenshots/` y referencialas aqui cuando esten disponibles.
