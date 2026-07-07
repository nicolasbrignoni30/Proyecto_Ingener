// mainMCP2515.cpp
#include <Arduino.h>
#include <SPI.h> 
#include "bms_can.h"  
#include "bms_parser.h"
#include "telemetria.h"


// Variables necesarias para crear el paquete
can_frame canMsgTx;
can_frame canMsgRx;

canid_t id = 0x00004200;
__u8 dlc = 8;
__u8 data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

byte sndStat = 0;
BmsData bms;

// Se define el modo de funcionamento, en este caso normal
ModoFuncionamiento modo = MODO_NORMAL;

void setup() {
    //  Inicializamos el monitor serie a alta velocidad
    Serial.begin(115200);
    while(!Serial); // Espera a que se abra la terminal (útil en algunas placa)

    // Encendemos el motor del bus SPI global de la placa
    SPI.begin(); 
    
    // 3. Inicializamos tu módulo CAN (configura pines, ISR y Modo Loopback)
    bmsCanInit(modo);

    // 4. Inicializamos wifi y mqtt.
    connectWiFi();
    connectMQTT();

    mkMsg(id, data, dlc, &canMsgTx);
}

bool listening = true;
bool flag_recibido = false;

unsigned long lastPublishBms = 0;
unsigned long lastListenBms = 0;

uint8_t contador_frames = 0;
const uint8_t num_frames = 3; //Tengo entendido que el BMS va a mandar 10 frames, algunos de los cuales no nos interesan.

void loop() {
    // 1. MANTENER VIVO EL PROTOCOLO MQTT (Ejecutarlo siempre para no perder la sesión)
    if (checkMQTTConnection()) {
        loopMQTT(); // Adentro corre client.loop()
    }

    // 2. PETICIÓN Y PARSEO DEL BMS (Con Timeout Correcto)
    if (millis() - lastListenBms > LISTEN_INTERVAL_BMS) {
        lastListenBms = millis();
        listening = true;
        limpiarBuffers_Rx(); //Por las dudas se limpian los buffers de recepcion del MCP2515.

        // Se manda la trama 0x4200 para leer el BMS 
        sndStat = bmsSend(&canMsgTx);
        while (contador_frames < num_frames) {
            flag_recibido = bmsReceive(&canMsgRx, listening);
            if (flag_recibido) {
                uint16_t id = (uint16_t)(canMsgRx.can_id);
                Serial.println(id, HEX);
                bms_parse_can(id, canMsgRx.data, bms);
                flag_recibido = false;
                contador_frames = contador_frames + 1;
            }
        }
        if (contador_frames >= num_frames){
            listening = false;
            contador_frames = 0;
        }
    }

    // 3. PUBLICACIÓN DE TELEMETRÍA (Cada PUBLISH_INTERVAL_BMS)
    if (millis() - lastPublishBms > PUBLISH_INTERVAL_BMS) {
        lastPublishBms = millis(); // Actualizamos el timer antes por seguridad
        
        // Verificación de conexiones de forma secuencial pero rápida
        if (!checkWiFiConnection()) { 
            connectWiFi(); // Trata de conectar (Ojalá no bloqueante)
        }
        
        if (checkWiFiConnection() && !checkMQTTConnection()) {
            connectMQTT(); // Trata de conectar al broker
        }
        
        // Si todo está en orden, publicamos a ThingsBoard
        if (checkMQTTConnection()) {
            // --- INICIO DE LA MEDICIÓN DE TIEMPO ---
            unsigned long t_inicio = millis(); 
            
            publishTelemetryBMS(bms);
            
            unsigned long t_fin = millis();
            unsigned long tiempo_demorado = t_fin - t_inicio;
            // --- FIN DE LA MEDICIÓN DE TIEMPO ---

            // Imprimimos el resultado en la terminal serie
            Serial.print("Tiempo de ejecucion de publishTelemetryBMS: ");
            Serial.print(tiempo_demorado);
            Serial.println(" ms");
        }
    }
}