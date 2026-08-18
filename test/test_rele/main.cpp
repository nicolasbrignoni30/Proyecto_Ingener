#include <Arduino.h>

// Definición de pines GPIO
const int PIN_VENTILADOR_1 = 12;
const int PIN_VENTILADOR_2 = 14;

// Variables para almacenar el estado actual de cada salida (falso = APAGADO, verdadero = ENCENDIDO)
bool estadoPin12 = false;
bool estadoPin14 = false;

// Variables para el temporizado no bloqueante del GPIO 14
unsigned long tiempoAnterior = 0;
const unsigned long INTERVALO_CONMUTACION = 5000; // 5000 ms = 5 segundos

void setup() {
  // Inicialización de la comunicación serial a 115200 baudios
  Serial.begin(115200);
  while (!Serial) {
    ; // Espera a que el puerto serie se conecte (necesario en algunas placas)
  }

  // Configuración de los pines seleccionados como salidas digitales
  pinMode(PIN_VENTILADOR_1, OUTPUT);
  pinMode(PIN_VENTILADOR_2, OUTPUT);

  // Asegurar que arranquen en estado BAJO (APAGADO) por seguridad
  digitalWrite(PIN_VENTILADOR_1, LOW);
  digitalWrite(PIN_VENTILADOR_2, LOW);

  Serial.println("--- Control de Contactores Listo ---");
  Serial.println("Presiona '1' para alternar GPIO 12");
  Serial.println("GPIO 14 conmuta automaticamente cada 5 segundos");
}

void loop() {
  // --- Conmutación automática de GPIO 14 cada 5 segundos ---
  unsigned long tiempoActual = millis();
  if (tiempoActual - tiempoAnterior >= INTERVALO_CONMUTACION) {
    tiempoAnterior = tiempoActual;

    estadoPin14 = !estadoPin14;
    digitalWrite(PIN_VENTILADOR_2, estadoPin14 ? HIGH : LOW);

    Serial.print("GPIO 14 cambiado a: ");
    Serial.println(estadoPin14 ? "ALTO (ENCENDIDO)" : "BAJO (APAGADO)");
  }

  // --- Verificar si hay datos entrantes en el búfer del puerto serie ---
  if (Serial.available() > 0) {
    // Leer el carácter entrante
    char tecla = Serial.read();

    // Evaluar la tecla presionada
    switch (tecla) {
      case '1':
        // Invierte el estado booleano actual
        estadoPin12 = !estadoPin12; 
        digitalWrite(PIN_VENTILADOR_1, estadoPin12 ? HIGH : LOW);
        
        Serial.print("GPIO 12 cambiado a: ");
        Serial.println(estadoPin12 ? "ALTO (ENCENDIDO)" : "BAJO (APAGADO)");
        break;

      // Filtrar saltos de línea (\n) o retornos de carro (\r) que envían las terminales automáticamente
      case '\n':
      case '\r':
        break;

      default:
        // Mensaje opcional para capturar cualquier otra tecla no válida
        Serial.print("Tecla no asignada: ");
        Serial.println(tecla);
        break;
    }
  }
}