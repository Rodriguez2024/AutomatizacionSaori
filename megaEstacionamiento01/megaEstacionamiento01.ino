#include <Servo.h>

Servo servoEntrada;
Servo servoSalida;

// Pines para los sensores de entrada y salida (puedes mantener los mismos)
int trigEntrada = 13;
int echoEntrada = 12;
int trigSalida = 4;
int echoSalida = 6;

// FLAGS para entrada/salida
bool autoEntradaDetectado = false;
bool autoSalidaDetectado = false;

// LEDs para entrada/salida
int ledEntradaDetect = 7;  // Se enciende cuando se manda 40
int ledSalidaDetect = 5;   // Se enciende cuando se manda 
int ledEntradaOk = 11;     // Se enciende cuando se recibe 1
int ledSalidaOk = 10;      // Se enciende cuando se recibe 2

// Configuración de los 6 cajones de estacionamiento
struct Cajon {
  int trig;
  int echo;
  int ledVerde;
  int ledRojo;
  bool ocupado;
};

// Definición de pines para los 6 cajones en el MEGA
Cajon cajones[6] = {
  { 22, 23, 24, 25, false },  // Cajón 1
  { 26, 27, 28, 29, false },  // Cajón 2
  { 30, 31, 32, 33, false },  // Cajón 3
  { 34, 35, 36, 37, false },  // Cajón 4
  { 38, 39, 40, 41, false },  // Cajón 5
  { 42, 43, 44, 45, false }   // Cajón 6oo
};

const int UMBRAL_OCUPADO = 5;  // cm - ajusta según necesidad
int cajonesOcupados = 0;
const int TOTAL_CAJONES = 6;

long medirDistancia(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duracion = pulseIn(echo, HIGH, 30000);  // Timeout de 30ms
  if (duracion == 0) return 100;               // Retorna 100cm si hay timeout

  return duracion * 0.034 / 2;
}

void controlarLEDsCajon(int cajonIndex, bool ocupado) {
  if (ocupado) {
    digitalWrite(cajones[cajonIndex].ledVerde, LOW);
    digitalWrite(cajones[cajonIndex].ledRojo, HIGH);
  } else {
    digitalWrite(cajones[cajonIndex].ledVerde, HIGH);
    digitalWrite(cajones[cajonIndex].ledRojo, LOW);
  }
}

// Función para enviar senal del estado de un cajón
void enviarSenalCajon(int cajonIndex, bool ocupado) {
  // Aquí puedes modificar el formato de la senal según lo que necesites
  // Por ejemplo, podrías enviar un JSON o un string simple
  Serial.print("Cajon:");
  Serial.print(cajonIndex + 1);
  Serial.print(",Estado:");
  Serial.println(ocupado ? "OCUPADO" : "LIBRE");
}

void actualizarCajon(int cajonIndex, bool ocupado) {
  if (ocupado != cajones[cajonIndex].ocupado) {
    cajones[cajonIndex].ocupado = ocupado;

    controlarLEDsCajon(cajonIndex, ocupado);

    if (ocupado) {
      cajonesOcupados++;
    } else {
      cajonesOcupados--;
    }

    // Enviar senal del cambio de estado
    enviarSenalCajon(cajonIndex, ocupado);

    Serial.print("Total ocupados: ");
    Serial.print(cajonesOcupados);
    Serial.print("/");
    Serial.println(TOTAL_CAJONES);
  }
}

void setup() {
  Serial.begin(9600);

  // Configurar pines de entrada/salida
  pinMode(trigEntrada, OUTPUT);
  pinMode(echoEntrada, INPUT);
  pinMode(trigSalida, OUTPUT);
  pinMode(echoSalida, INPUT);

  pinMode(ledEntradaDetect, OUTPUT);
  pinMode(ledSalidaDetect, OUTPUT);
  pinMode(ledEntradaOk, OUTPUT);
  pinMode(ledSalidaOk, OUTPUT);

  // Configurar pines para los cajones
  for (int i = 0; i < TOTAL_CAJONES; i++) {
    pinMode(cajones[i].trig, OUTPUT);
    pinMode(cajones[i].echo, INPUT);
    pinMode(cajones[i].ledVerde, OUTPUT);
    pinMode(cajones[i].ledRojo, OUTPUT);

    // Inicializar LEDs (verde encendido = libre)
    controlarLEDsCajon(i, false);
  }

  servoEntrada.attach(9);
  servoSalida.attach(8);

  servoEntrada.write(45);
  servoSalida.write(45);

  // Apagar LEDs de entrada/salida
  digitalWrite(ledEntradaDetect, LOW);
  digitalWrite(ledSalidaDetect, LOW);
  digitalWrite(ledEntradaOk, LOW);
  digitalWrite(ledSalidaOk, LOW);
}

void loop() {
  // -------- LECTURA DE SENSORES DE CAJONES --------
  for (int i = 0; i < TOTAL_CAJONES; i++) {
    long distancia = medirDistancia(cajones[i].trig, cajones[i].echo);
 
   if ((distancia >= 2 && distancia <= 4) && cajonesOcupados < TOTAL_CAJONES) {
      bool ocupado = (distancia <= UMBRAL_OCUPADO);
      actualizarCajon(i, ocupado);
    }
  }

  // -------- SENSOR DE ENTRADA --------
  long dEntrada = medirDistancia(trigEntrada, echoEntrada);

  // Validar distancia correcta y que haya cajones libres
  if (!autoEntradaDetectado && dEntrada >= 2 && dEntrada <= 4 && cajonesOcupados < TOTAL_CAJONES) {
    Serial.println(40);
    autoEntradaDetectado = true;
    digitalWrite(ledEntradaDetect, HIGH);
    delay(200);  // Anti-rebote
  }

  // -------- SENSOR DE SALIDA --------
  long dSalida = medirDistancia(trigSalida, echoSalida);

  if (!autoSalidaDetectado && dSalida >= 2 && dSalida <= 4) {
    Serial.println(30);
    autoSalidaDetectado = true;
    digitalWrite(ledSalidaDetect, HIGH);
    delay(200);  // Anti-rebote
  }

  // -------- LECTURA SERIAL (OPERADOR) --------
  if (Serial.available()) {
    int codigo = Serial.parseInt();

    switch (codigo) {
      case 1:  // operador autorizó ENTRADA
        digitalWrite(ledEntradaOk, HIGH);
        for (int i = 45; i <= 110; i++) {
          servoEntrada.write(i);  // Abrir barrera
          delay(30);
        }

        delay (2000);

        for (int i = 110; i >=  45; i--) {
          servoEntrada.write(i);  // Cerrar barrera
          delay(30);          
        }

        // Tiempo para que pase el auto
        digitalWrite(ledEntradaOk, LOW);

        autoEntradaDetectado = false;
        digitalWrite(ledEntradaDetect, LOW);
        break;

      case 2:  // operador autorizó SALIDA
        digitalWrite(ledSalidaOk, HIGH);
        for (int i = 45; i <= 110; i++) {
          servoSalida.write(i);  // Abrir barrera
          delay(30);
        }

        delay (2000);

        for (int i = 110; i >=  45; i--) {
          servoSalida.write(i);  // Cerrar barrera
          delay(30);          
        }
        digitalWrite(ledSalidaOk, LOW);

        autoSalidaDetectado = false;
        digitalWrite(ledSalidaDetect, LOW);
        break;

      case 0:  // estacionamiento lleno o reset
        digitalWrite(ledEntradaOk, LOW);
        autoEntradaDetectado = false;
        digitalWrite(ledEntradaDetect, LOW);
        break;
    }
  }

  delay(100);  // Espera entre ciclos de lectura
}