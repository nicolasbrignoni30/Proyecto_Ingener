#include <Arduino.h>
#include <ModbusMaster.h>
#include "inverter.h"  
#include "inverter_parser.h"
#include "telemetria.h"

InvData inv; 

bool firmwarePublicado = false;

void setup() {
    Serial.begin(115200);
    delay(500);

    // Se inicializa el inversor.
    Serial2.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    inverterInit(Serial2, RS485_DE_RE_PIN); 

    //Aca se ponen los 2kW en el registro 353 (setpoint).
    //Se podria hacer en 'inverterInit' pero aca queda mas explicito.
    inverterWrite(REG_SET_POWER, SET_POWER_RAW);

    // Inicializar redes
    connectWiFi();
    connectMQTT();

    Serial.println("[MAIN] Sistema inicializado correctamente.");
}

unsigned long lastPublishInv = 0;

void loop() {
    // 0. MANTENER VIVO EL PROTOCOLO MQTT (Ejecutarlo siempre para no perder la sesión)
    if (checkMQTTConnection()) {
        loopMQTT(); // Adentro corre client.loop()
    }

    // Leer y mandar firmware una sola vez.
    if (!firmwarePublicado) {
        Serial.println("[MAIN] Leyendo versión de firmware por única vez...");
        
        // Pasamos inv.firm directamente (la subestructura interna)
        readFirmwareVersion(inv.firm); 
        
        // Publicamos usando tu método por strings
        publishTelemetryInv(inv, "Firm");
        
        firmwarePublicado = true; // Bloqueamos para que no vuelva a entrar
    }

    
    //polling y telemetria.
        if (millis() - lastPublishInv >= PUBLISH_INTERVAL_INV) {
            lastPublishInv = millis();

            pollModbus(inv);

                // Verificación de conexiones de forma secuencial pero rápida
            if (!checkWiFiConnection()) { 
                connectWiFi(); // Trata de conectar (Ojalá no bloqueante)
            }
            
            if (checkWiFiConnection() && !checkMQTTConnection()) {
                connectMQTT(); // Trata de conectar al broker
            }
            
            // Si todo está en orden, publicamos a ThingsBoard
            if (checkMQTTConnection()) {
                // Publicamos los bloques dinámicos de manera segmentada usando tu lógica
                publishTelemetryInv(inv, "StatusData");
                publishTelemetryInv(inv, "AcData");
                publishTelemetryInv(inv, "DcData");
                publishTelemetryInv(inv, "GridData");
                
                #ifdef INVERTER_PROTOCOL_V3
                // Solo si el firmware reportó protocolo v3 mandamos la carga
                if (inv.firm.fw_rtu_protocol >= 30) {
                    publishTelemetryInv(inv, "LoadData");
                }
                #endif
            }
        }
}