#include <Servo.h>

Servo servoEntrada;
Servo servoSalida;

int trigEntrada = 13;
int echoEntrada = 12;

int trigSalida = 4;
int echoSalida = 6;

// FLAGS
bool autoEntradaDetectado = false;
bool autoSalidaDetectado = false;

// LEDs
int ledEntradaDetect = 7;  // Se enciende cuando se manda 40
int ledSalidaDetect  = 5;  // Se enciende cuando se manda 30
int ledEntradaOk     = 11; // Se enciende cuando se recibe 1
int ledSalidaOk      = 10; // Se enciende cuando se recibe 2

long medirDistancia(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duracion = pulseIn(echo, HIGH, 25000);
  if (duracion == 0) return -1;

  return duracion * 0.034 / 2;
}

void setup() {
  Serial.begin(9600);

  pinMode(trigEntrada, OUTPUT);
  pinMode(echoEntrada, INPUT);

  pinMode(trigSalida, OUTPUT);
  pinMode(echoSalida, INPUT);

  pinMode(ledEntradaDetect, OUTPUT);
  pinMode(ledSalidaDetect, OUTPUT);
  pinMode(ledEntradaOk, OUTPUT);
  pinMode(ledSalidaOk, OUTPUT);

  servoEntrada.attach(9);
  servoSalida.attach(8);

  servoEntrada.write(45);
  servoSalida.write(45);

  digitalWrite(ledEntradaDetect, LOW);
  digitalWrite(ledSalidaDetect, LOW);
  digitalWrite(ledEntradaOk, LOW);
  digitalWrite(ledSalidaOk, LOW);
}

void loop() {

  // -------- SENSOR DE ENTRADA --------
  long dEntrada = medirDistancia(trigEntrada, echoEntrada);

  // Validar distancia correcta
  if (!autoEntradaDetectado && dEntrada >= 2 && dEntrada <= 4) {  // rango
    if (!autoEntradaDetectado) {
      Serial.println(40);
      autoEntradaDetectado = true;
      digitalWrite(ledEntradaDetect, HIGH);
      delay(200); // Anti-rebote del ultrasónico
    }
  }

  // -------- SENSOR DE SALIDA --------
  long dSalida = medirDistancia(trigSalida, echoSalida);

  if (!autoSalidaDetectado && dSalida >= 2 && dSalida <= 4) {
    //if (dSalida != -1 && dSalida < 20) {   // 20 cm de rango
      Serial.println(30);
      autoSalidaDetectado = true;
      digitalWrite(ledSalidaDetect, HIGH);
      delay(200);  // Anti-rebote
  }

  // -------- LECTURA SERIAL --------
  if (Serial.available()) {
    int codigo = Serial.parseInt();

    switch (codigo) {

      case 1:   // operador autorizó ENTRADA
        digitalWrite(ledEntradaOk, HIGH);
        delay(300);
        digitalWrite(ledEntradaOk, LOW);

        autoEntradaDetectado = false;
        digitalWrite(ledEntradaDetect, LOW);
        break;

      case 2:   // operador autorizó SALIDA
        digitalWrite(ledSalidaOk, HIGH);
        delay(300);
        digitalWrite(ledSalidaOk, LOW);

        autoSalidaDetectado = false;
        digitalWrite(ledSalidaDetect, LOW);
        break;
      case 0:   // estacionamiento lleno o no hay autos 
        digitalWrite(ledEntradaOk, LOW);
        autoEntradaDetectado = false;
        digitalWrite(ledEntradaDetect, LOW);
        break;
    }
  }

  delay(60);
}
