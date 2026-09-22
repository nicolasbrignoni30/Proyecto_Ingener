#include <Arduino.h>

// Definición de pines GPIO
const int PIN_VENTILADOR = 14;

// Variables para almacenar el estado actual de cada salida (falso = APAGADO, verdadero = ENCENDIDO)
bool estadoPin12 = false;

void setup() {
  // Inicialización de la comunicación serial a 115200 baudios
  Serial.begin(115200);
  while (!Serial) {
    ; // Espera a que el puerto serie se conecte (necesario en algunas placas)
  }

  // Configuración de los pines seleccionados como salidas digitales
  pinMode(PIN_VENTILADOR, OUTPUT);

  // Asegurar que arranquen en estado BAJO (APAGADO) por seguridad
  digitalWrite(PIN_VENTILADOR, LOW);

  Serial.println("--- Control de Contactores Listo ---");
  Serial.println("Presiona '1' para alternar GPIO 12");
}

void loop() {
  // --- Verificar si hay datos entrantes en el búfer del puerto serie ---
  if (Serial.available() > 0) {
    // Leer el carácter entrante
    char tecla = Serial.read();

    // Evaluar la tecla presionada
    switch (tecla) {
      case '1':
        // Invierte el estado booleano actual
        estadoPin12 = !estadoPin12; 
        digitalWrite(PIN_VENTILADOR, estadoPin12 ? HIGH : LOW);
        
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