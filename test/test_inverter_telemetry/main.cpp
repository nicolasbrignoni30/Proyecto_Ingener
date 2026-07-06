#include <Arduino.h>
#include <ModbusMaster.h>
#include "inverter.h"  
#include "inverter_parser.h"
#include "telemetria.h"

InvData inv; 

bool firmwarePublicado = false;

void setup() {
    Serial.begin(115200);
    
    // Inicializar hardware del inversor (ajustá los pines a tu placa)
    // Supongamos que pasás el puerto Serial2 y el pin de Control de Dirección RS485
    inverterInit(Serial2, 4); 

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

    // 1. LEER Y MANDAR FIRMWARE (Una sola vez al arrancar y tener red)
    if (!firmwarePublicado) {
        Serial.println("[MAIN] Leyendo versión de firmware por única vez...");
        
        // Pasamos inv.firm directamente (la subestructura interna)
        readFirmwareVersion(inv.firm); 
        
        // Publicamos usando tu método por strings
        publishTelemetryInv(inv, "Firm");
        
        firmwarePublicado = true; // Bloqueamos para que no vuelva a entrar
    }

    
    // 2. POLLING Y TELEMETRÍA DINÁMICA (Cada intervalo de tiempo)
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