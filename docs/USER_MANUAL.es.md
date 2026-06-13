# Manual de Usuario SmartGym

Idioma: [English](USER_MANUAL.md) | Espanol

Este manual explica como usar el sistema SmartGym Adaptive Training como evaluador, atleta o usuario de demo.

## 1. Encender El Dispositivo

1. Conecta energia a la pantalla ESP32-S3.
2. Espera a que aparezca la pantalla principal.
3. Permite que el dispositivo conecte Wi-Fi, Firebase, RFID y servicios de tiempo.
4. No inicies un entrenamiento hasta que el dispositivo llegue al estado listo.

## 2. Login Por RFID

1. Acerca la tarjeta RFID al lector.
2. La pantalla muestra un popup de carga.
3. Espera hasta que el popup se cierre.

El popup permanece visible mientras el dispositivo carga:

- Perfil de usuario.
- Calibracion del usuario para la maquina activa.
- Recomendacion para usuario y maquina activos.
- Mapeo ROM calibrado.

No se debe presionar START o CALIBRATE mientras el popup de carga esta activo.

## 3. Pantalla Principal

La pantalla principal muestra estado de sesion, grafica de movimiento, carga de pin, recomendacion, repeticiones, series y botones de accion.

Terminos importantes:

- Pin load: posicion fisica del pin de peso en la maquina.
- Recommended load: carga sugerida por SmartGym para el usuario activo.

La carga de pin pertenece a la maquina, no al usuario. No cambia automaticamente cuando otro usuario escanea RFID. La recomendacion es consejo especifico para el usuario.

## 4. Pin Load Vs Recommended Load

El sistema nunca mueve fisicamente el pin de peso. Si la pantalla recomienda 25.0 kg pero la maquina esta en 20.0 kg, el dispositivo muestra ambos valores:

- Pin load: 20.0 kg
- Recommended: 25.0 kg

Si el usuario quiere seguir la recomendacion, debe mover fisicamente el pin y actualizar el valor mostrado con los botones tactiles.

## 5. Cambiar Pin Load

Usa los cuatro botones de peso:

- `-5`
- `-2.5`
- `+2.5`
- `+5`

Estos botones actualizan la carga de pin mostrada. No cambian la recomendacion.

El pin load puede cambiar antes o durante el entrenamiento. Si cambia durante el entrenamiento, las repeticiones posteriores se registran con la nueva carga.

## 6. Iniciar Un Entrenamiento

1. Escanea RFID y espera a que termine la carga.
2. Revisa el pin load.
3. Ajusta fisicamente el pin de la maquina si hace falta.
4. Ajusta el pin load en pantalla para que coincida con el pin fisico.
5. Presiona START.

La sesion inicia usando exactamente el pin load mostrado.

## 7. Seguir La Grafica

La grafica muestra el movimiento a traves del rango del ejercicio. El firmware usa ROM, o rango de movimiento, para detectar repeticiones.

Las pistas para usuario se enfocan en tiempo:

- Tiempo de subida.
- Tiempo de bajada.
- Pausa arriba.
- Pausa abajo.

La interfaz principal evita mostrar valores tecnicos crudos como `%ROM/s`.

## 8. Repeticiones Y Series

El firmware detecta repeticiones desde la senal ROM. Una repeticion debe moverse por suficiente rango y pasar reglas internas de validacion.

La pantalla muestra:

- Repeticiones actuales en la serie.
- Serie actual.
- Estado de entrenamiento.
- Timer o progreso cuando esta disponible.

Si una repeticion no cuenta, las causas pueden ser ROM corto, movimiento ruidoso o no llegar a umbrales de arriba/abajo.

## 9. Descanso Y Finalizacion

Cuando se completan las repeticiones objetivo de una serie, el sistema marca la serie como completa. Segun el numero objetivo de series, la sesion puede terminar automaticamente o continuar a la siguiente serie.

El usuario tambien puede terminar manualmente presionando FINISH.

## 10. Resumen Y Sync

Despues de terminar, aparece la pantalla de resumen. Puede mostrar:

- Sesion completa.
- Pin load usado.
- Nueva recomendacion.
- Estado de guardado/subida.

Estados tipicos:

- Saving session...
- Uploading core...
- Uploading details...
- Uploading reps...
- Session saved.
- Saving in background.

Si la sincronizacion sigue corriendo, el dispositivo puede guardar en segundo plano. No se debe apagar inmediatamente despues de entrenar si la pantalla aun indica guardado.

## 11. Calibracion

Usa calibracion cuando:

- Un usuario nuevo use la maquina.
- El ROM se vea incorrecto.
- Falten recomendaciones.
- El sistema indique calibrate first.
- Cambie la configuracion del ejercicio.

## Pasos De Calibracion

1. Presiona CALIBRATE.
2. Lee el titulo e instruccion del paso.
3. Mueve el pin fisico a la carga sugerida si aplica.
4. Usa los cuatro botones de pin load si el valor en pantalla no coincide con el pin fisico.
5. Toca LOAD SET.
6. Haz 3 a 5 repeticiones suaves.
7. Sigue cualquier instruccion de siguiente carga.
8. Guarda el resultado al terminar.

La pantalla de calibracion muestra:

- Pin load.
- Carga sugerida.
- Botones de ajuste de peso.
- Feedback de repeticiones.
- Acciones de guardar/cancelar.

Si el pin load difiere de la carga sugerida, el usuario puede continuar, pero el dispositivo registra que se eligio un pin fisico distinto.

## 12. Recomendaciones

Las recomendaciones se generan desde calibracion e historial de sesiones. Son consejo solamente. El sistema no cambia automaticamente el pin fisico.

Fuentes posibles:

- Calibracion.
- Ultima sesion.
- Default de maquina.
- Sin recomendacion.

## 13. Troubleshooting Rapido

### RFID No Detectado

- Acerca mas la tarjeta al lector.
- Espera a que termine upload/sync.
- Intenta escanear de nuevo.

### Usuario Sigue Cargando

- Espera a que cierre el popup.
- Si falla la lectura cloud, el dispositivo debe hacer fallback seguro.

### Repeticion No Contada

- Usa ROM completo.
- Muevete suavemente.
- Llega a posiciones de arriba y abajo.
- Recalibra si el mapeo ROM se ve mal.

### Grafica Incorrecta

- Recalibra ROM.
- Revisa conexion del sensor de movimiento.
- Confirma que no cambio la configuracion de la maquina.

### Sync Sigue Corriendo

- Mantén el dispositivo encendido.
- Espera "Session saved" si es posible.
- El dispositivo puede seguir guardando en segundo plano.

### Recomendacion No Cambia Pin Load

Esto es esperado. Pin load es el estado fisico de la maquina. El usuario debe mover el pin y actualizar pantalla manualmente.

### Wi-Fi O Firebase

- Revisa credenciales Wi-Fi.
- Revisa URL/configuracion de Firebase RTDB.
- Confirma conectividad a internet.
- Usa logs seriales para detalles.
