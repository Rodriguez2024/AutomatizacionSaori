#include <Servo.h>

Servo servoEntrada;
Servo servoSalida;

int trigEntrada = 13;
int echoEntrada = 12;
int ledVerde = 7;
int ledAzul = 10;

int trigSalida = 4;
int echoSalida = 6;

// FLAGS
bool autoEntradaDetectado = false;
bool autoSalidaDetectado = false;

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

  pinMode(ledVerde, OUTPUT);
  pinMode(ledAzul, OUTPUT);

  servoEntrada.attach(9);
  servoSalida.attach(8);

  servoEntrada.write(45);
  servoSalida.write(45);
}

void loop() {

  // -------- SENSOR ENTRADA --------
  long dEntrada = medirDistancia(trigEntrada, echoEntrada);

  if (!autoEntradaDetectado && dEntrada >= 2 && dEntrada <= 4) {
      Serial.println(40);       // Aviso a la PC
      autoEntradaDetectado = true;
  }

  // -------- SENSOR SALIDA --------
  long dSalida = medirDistancia(trigSalida, echoSalida);

  if (!autoSalidaDetectado && dSalida >= 2 && dSalida <= 4) {
      Serial.println(30);       // Aviso a la PC
      autoSalidaDetectado = true;
  }

  // -------- LECTURA SERIAL --------
  if (Serial.available()) {
    int codigo = Serial.parseInt();

    switch (codigo) {
      case 1:   // operador autorizó ENTRADA
        digitalWrite(ledVerde, HIGH);
        delay (500);
        digitalWrite(ledVerde, LOW);
        plumaEntrada();
        autoEntradaDetectado = false;
        break;

      case 2:   // operador autorizó SALIDA
        digitalWrite(ledAzul, HIGH);
        delay (500);
        digitalWrite(ledAzul, LOW);
        plumaSalida();
        autoSalidaDetectado = false;
        break;
    }
  }

  delay(60);
}

// ----------------- FUNCIONES -----------------
void plumaEntrada() {
  for (int a = 45; a <= 180; a++) {
    servoEntrada.write(a);
    delay(15);
  }
  delay(500);
  for (int a = 180; a >= 45; a--) {
    servoEntrada.write(a);
    delay(15);
  }
}

void plumaSalida() {
  for (int a = 45; a <= 180; a++) {
    servoSalida.write(a);
    delay(15);
  }
  delay(500);
  for (int a = 180; a >= 45; a--) {
    servoSalida.write(a);
    delay(15);
  }
}