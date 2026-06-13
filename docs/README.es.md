# Documentacion SmartGym

Idioma: [English](README.md) | Espanol

Esta carpeta contiene el paquete de documentacion del prototipo SmartGym v1.0. Cada guia importante esta disponible como Markdown editable; algunas tambien tienen PDF formateado.

## Empieza Aqui

- [Empieza Aqui](START_HERE.es.md)
- [README del proyecto](../README.es.md)
- [Manual de usuario](USER_MANUAL.es.md) | [PDF ingles](USER_MANUAL.pdf)
- [Arquitectura del sistema](SYSTEM_ARCHITECTURE.es.md) | [PDF ingles](SYSTEM_ARCHITECTURE.pdf)
- [Guia de desarrollador](DEVELOPER_GUIDE.es.md) | [PDF ingles](DEVELOPER_GUIDE.pdf)
- [Guia de medicion y calibracion](MEASUREMENT_AND_CALIBRATION_GUIDE.es.md)
- [Guia de Firebase](FIREBASE_GUIDE.es.md) | [PDF ingles](FIREBASE_GUIDE.pdf)
- [Troubleshooting](TROUBLESHOOTING.es.md) | [PDF ingles](TROUBLESHOOTING.pdf)

## Documentos De Referencia

| Documento | Markdown | PDF |
| --- | --- | --- |
| Empieza Aqui | [Abrir](START_HERE.es.md) | Se puede generar localmente |
| Guia de medicion y calibracion | [Abrir](MEASUREMENT_AND_CALIBRATION_GUIDE.es.md) | Se puede generar localmente |
| Guia de seed Firebase para dashboard | [Abrir](FIREBASE_DASHBOARD_SEED_GUIDE.es.md) | Se puede generar localmente |

## Encontrar Informacion Por Tarea

| Tarea | Mejor documento |
| --- | --- |
| Entender el proyecto completo rapidamente | [Empieza Aqui](START_HERE.es.md) |
| Operar el dispositivo fisico | [Manual de usuario](USER_MANUAL.es.md) |
| Explicar ROM, calibracion, formulas, ruido y errores | [Guia de medicion y calibracion](MEASUREMENT_AND_CALIBRATION_GUIDE.es.md) |
| Entender modulos de firmware y flujo runtime | [Arquitectura del sistema](SYSTEM_ARCHITECTURE.es.md) |
| Compilar, subir, depurar o modificar firmware | [Guia de desarrollador](DEVELOPER_GUIDE.es.md) |
| Entender rutas Firebase y fases de subida | [Guia de Firebase](FIREBASE_GUIDE.es.md) |
| Entender datos seed/demo de Firebase y dashboard | [Guia de seed Firebase para dashboard](FIREBASE_DASHBOARD_SEED_GUIDE.es.md) |
| Diagnosticar build, RFID, ROM, sync o dashboard | [Troubleshooting](TROUBLESHOOTING.es.md) |
| Encontrar archivos CAD/STL | [CAD de hardware](../hardware/cad/README.es.md) |
| Ejecutar dashboard | [Dashboard](../dashboard/README.es.md) |
| Usar prototipo ToF independiente | [Weight Detection](../weight_detection/README.es.md) |

## Orden Recomendado

1. Lee [Empieza Aqui](START_HERE.es.md) para ver el mapa del proyecto.
2. Lee el [Manual de usuario](USER_MANUAL.es.md) para entender uso del dispositivo.
3. Lee la [Guia de medicion y calibracion](MEASUREMENT_AND_CALIBRATION_GUIDE.es.md) para entender ROM, reps, calibracion y errores.
4. Lee [Arquitectura del sistema](SYSTEM_ARCHITECTURE.es.md) para entender el flujo completo.
5. Lee la [Guia de desarrollador](DEVELOPER_GUIDE.es.md) antes de modificar firmware.
6. Lee la [Guia de Firebase](FIREBASE_GUIDE.es.md) antes de importar, editar o migrar datos.
7. Lee la [Guia de seed Firebase para dashboard](FIREBASE_DASHBOARD_SEED_GUIDE.es.md) antes de cargar datos demo.
8. Usa [Troubleshooting](TROUBLESHOOTING.es.md) durante demos y pruebas de hardware.

## Regenerar PDFs

Ejecuta esto desde la raiz del repositorio:

```powershell
python tools/generate_docs_pdfs.py
```
