#include <Wire.h>
#include "Adafruit_VL53L0X.h"

// Standalone prototype for detecting a selectorized weight-stack position
// with a VL53L0X time-of-flight distance sensor.

Adafruit_VL53L0X lox = Adafruit_VL53L0X();

const int NUM_MUESTRAS = 30;
const int SDA_PIN = 21;
const int SCL_PIN = 22;

float distanciaPromedioCm = 0.0f;

int pesoDetectado = 0;
int stackDetectado = 0;
int pesoFinal = 0;

const int NUM_CONFIRMACIONES = 5;
int ultimoPesoTemporal = 0;
int contadorConfirmacion = 0;
long sumaPesoFinal = 0;

String estado = "Inicializando...";
String estadoPesoFinal = "Esperando confirmacion...";

void iniciarVL53L0X() {
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!lox.begin()) {
    Serial.println("Error: No se detecto el VL53L0X.");
    Serial.println("Revisa conexiones:");
    Serial.println("VCC -> 3.3V");
    Serial.println("GND -> GND");
    Serial.println("SDA -> GPIO 21");
    Serial.println("SCL -> GPIO 22");

    while (1) {
      delay(100);
    }
  }

  Serial.println("VL53L0X detectado correctamente.");
}

float leerDistanciaCm() {
  VL53L0X_RangingMeasurementData_t measure;

  lox.rangingTest(&measure, false);

  if (measure.RangeStatus != 4) {
    return measure.RangeMilliMeter / 10.0f;
  }

  return -1.0f;
}

float leerPromedioDistanciaCm() {
  float suma = 0.0f;
  int lecturasValidas = 0;

  for (int i = 0; i < NUM_MUESTRAS; i++) {
    float d = leerDistanciaCm();

    if (d > 0.0f && d < 200.0f) {
      suma += d;
      lecturasValidas++;
    }

    delay(20);
  }

  if (lecturasValidas == 0) {
    return -1.0f;
  }

  return suma / lecturasValidas;
}

void detectarPesoPorDistancia(float distanciaCm) {
  pesoDetectado = 0;
  stackDetectado = 0;

  if (distanciaCm < 0.0f) {
    estado = "Lectura invalida o fuera de rango";
    return;
  }

  if (distanciaCm >= 0.00f && distanciaCm < 2.00f) {
    pesoDetectado = 220;
    stackDetectado = 1;
  } else if (distanciaCm >= 4.00f && distanciaCm < 6.00f) {
    pesoDetectado = 205;
    stackDetectado = 2;
  } else if (distanciaCm >= 7.00f && distanciaCm < 8.00f) {
    pesoDetectado = 190;
    stackDetectado = 3;
  } else if (distanciaCm >= 8.20f && distanciaCm < 9.00f) {
    pesoDetectado = 175;
    stackDetectado = 4;
  } else if (distanciaCm >= 9.90f && distanciaCm < 11.00f) {
    pesoDetectado = 160;
    stackDetectado = 5;
  } else if (distanciaCm >= 12.00f && distanciaCm < 13.50f) {
    pesoDetectado = 145;
    stackDetectado = 6;
  } else if (distanciaCm >= 14.50f && distanciaCm < 15.50f) {
    pesoDetectado = 130;
    stackDetectado = 7;
  } else if (distanciaCm >= 16.80f && distanciaCm < 17.50f) {
    pesoDetectado = 115;
    stackDetectado = 8;
  } else if (distanciaCm >= 18.60f && distanciaCm < 19.80f) {
    pesoDetectado = 100;
    stackDetectado = 9;
  } else if (distanciaCm >= 20.60f && distanciaCm < 21.50f) {
    pesoDetectado = 85;
    stackDetectado = 10;
  } else if (distanciaCm >= 21.90f && distanciaCm < 22.90f) {
    pesoDetectado = 70;
    stackDetectado = 11;
  } else if (distanciaCm >= 23.00f && distanciaCm < 24.00f) {
    pesoDetectado = 55;
    stackDetectado = 12;
  } else if (distanciaCm >= 24.20f && distanciaCm < 24.60f) {
    pesoDetectado = 40;
    stackDetectado = 13;
  } else if (distanciaCm >= 24.61f && distanciaCm < 25.25f) {
    pesoDetectado = 25;
    stackDetectado = 14;
  } else if (distanciaCm >= 25.35f && distanciaCm < 41.25f) {
    pesoDetectado = 10;
    stackDetectado = 15;
  } else {
    pesoDetectado = 0;
    stackDetectado = 0;
    estado = "Distancia fuera de los rangos configurados";
    return;
  }

  estado = "Peso detectado correctamente";
}

void actualizarPesoFinal() {
  if (pesoDetectado <= 0) {
    ultimoPesoTemporal = 0;
    contadorConfirmacion = 0;
    sumaPesoFinal = 0;
    estadoPesoFinal = "Esperando peso valido...";
    return;
  }

  if (pesoDetectado != ultimoPesoTemporal) {
    ultimoPesoTemporal = pesoDetectado;
    contadorConfirmacion = 1;
    sumaPesoFinal = pesoDetectado;
    estadoPesoFinal = "Confirmando nuevo peso...";
    return;
  }

  contadorConfirmacion++;
  sumaPesoFinal += pesoDetectado;

  if (contadorConfirmacion >= NUM_CONFIRMACIONES) {
    pesoFinal = sumaPesoFinal / contadorConfirmacion;
    estadoPesoFinal = "Peso final confirmado";

    contadorConfirmacion = NUM_CONFIRMACIONES;
    sumaPesoFinal = static_cast<long>(pesoFinal) * NUM_CONFIRMACIONES;
  }
}

void actualizarMedicionPeso() {
  distanciaPromedioCm = leerPromedioDistanciaCm();
  detectarPesoPorDistancia(distanciaPromedioCm);
  actualizarPesoFinal();
}

void imprimirDatosPeso() {
  Serial.println("==============================");

  Serial.print("Distancia promedio: ");
  if (distanciaPromedioCm > 0.0f) {
    Serial.print(distanciaPromedioCm, 2);
    Serial.println(" cm");
  } else {
    Serial.println("Lectura invalida");
  }

  Serial.print("Peso detectado: ");
  if (pesoDetectado > 0) {
    Serial.print(pesoDetectado);
    Serial.println(" lb");
  } else {
    Serial.println("No detectado");
  }

  Serial.print("Valor final: ");
  if (pesoFinal > 0) {
    Serial.print(pesoFinal);
    Serial.println(" lb");
  } else {
    Serial.println("No confirmado");
  }

  Serial.print("Confirmacion: ");
  Serial.print(contadorConfirmacion);
  Serial.print(" / ");
  Serial.println(NUM_CONFIRMACIONES);

  Serial.print("Stack detectado: ");
  if (stackDetectado > 0) {
    Serial.println(stackDetectado);
  } else {
    Serial.println("Ninguno");
  }

  Serial.print("Estado: ");
  Serial.println(estado);

  Serial.print("Estado valor final: ");
  Serial.println(estadoPesoFinal);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Iniciando sistema de deteccion de peso...");
  iniciarVL53L0X();

  estado = "Sistema listo";
}

void loop() {
  actualizarMedicionPeso();
  imprimirDatosPeso();

  delay(100);
}
