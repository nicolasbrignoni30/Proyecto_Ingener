// =============================================================================
// main.cpp — Firmware final del banco de baterías
//
// Integra todos los módulos ya probados por separado en test/:
//   - bms_can        : lectura del BMS por CAN (MCP2515)
//   - dht_sensor     : temperatura/humedad ambiente
//   - thermal_control: máquina de estados de ventiladores / heating plates
//   - inverter       : lectura/escritura del inversor por Modbus RTU (RS485)
//   - telemetria     : WiFi + MQTT hacia ThingsBoard
//
// Loop principal:
//   1. Lee un lote de frames del BMS por CAN y actualiza `bms`.
//   2. Publica la telemetría del BMS apenas se actualiza.
//   3. Lee temperatura ambiente del DHT22.
//   4. Corre el control térmico (T_min/T_max del BMS + T ambiente del DHT);
//      si hace falta, pide el shutdown de emergencia al inversor.
//   5. Sondea el inversor por Modbus (periódico, no todos los loops porque
//      son varias transacciones Modbus bloqueantes) y publica su telemetría.
// =============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <string>

#include "bms_can.h"
#include "bms_parser.h"
#include "thermal_control.h"
#include "telemetria.h"
#include "dht_sensor.h"
#include "inverter.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------
ModoFuncionamiento modoCan = MODO_NORMAL;

can_frame canMsgRx;
BmsData   bms;
DhtData   dht;
InvData   datosInv;

unsigned long lastPollInv = 0;
unsigned long lastVerifyInv = 0;

// Último valor válido de temperatura ambiente — si el DHT falla una lectura,
// seguimos usando este en vez de mezclarlo con la temperatura de las celdas.
float lastTempAmb = 25.0f;

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

// Se llama por cada frame CAN que llega del BMS; va completando `bms`.
// Se hace de esta forma para lograr la independencia de modulos.
void onBmsFrame(can_frame* ptr_msg) {
    uint16_t id = (uint16_t)(*ptr_msg).can_id;
    bms_parse_can(id, (*ptr_msg).data, bms);
}

// thermal_control llama a esto para reportar reducción de potencia / shutdown.
// Publica a thingsboard la temperatura actual, y dos booleanos para poder hacer la logica
// de si hay que avisarle a EveMove que hay que bajar la potencia o directamente cortar
void publicarEstadoCooling(float T, bool bajar_pot, bool shut_down) {
    publishTemperature(T, bajar_pot, shut_down);
}

void init_all_defaults(){
    Serial2.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    inverter_init_defaults(RS485_SERIAL, RS485_DE_RE_PIN);
    thermal_thresholds_init_defaults();
};

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    while (!Serial); // espera a que se abra la terminal (útil en algunas placas)

    // --- CAN (BMS) ---
    SPI.begin();
    bmsCanInit(modoCan);
    Serial.println("[MAIN] BMS/CAN inicializado.");

    // --- Control térmico (pines de relé) ---
    thermalControlInit();
    Serial.println("[MAIN] Control termico inicializado.");

    // --- Sensor de temperatura/humedad ambiente ---
    dhtInit(DHT_PIN);
    Serial.println("[MAIN] Sensor DHT inicializado.");

    // --- WiFi + MQTT ---
    connectWiFi();
    connectMQTT();

    init_all_defaults();

    Serial.println("[MAIN] Setup finalizado con exito. Corriendo lazo...");
    Serial.println("--------------------------------------------------");
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
    // Como la conexion TCP tiene timeout hay que reconectar cada tanto
    // --- Conectividad ---
    if (!checkWiFiConnection()) {
        connectWiFi();
    }
    if (checkWiFiConnection() && !checkMQTTConnection()) {
        connectMQTT();
    }
    if (checkMQTTConnection()) {
        loopMQTT();
    }

    // --- 1 y 2: BMS por CAN + telemetría ---
    // Bloquea hasta juntar un lote de frames o hasta el timeout (LISTEN_INTERVAL_BMS).
    // `bms` queda actualizado con lo que haya llegado, aunque el lote se corte por timeout.
    bool loteCompleto = bmsReceiveBatchBlocking(&canMsgRx, 9, 2000, onBmsFrame);
    if (!loteCompleto) {
        Serial.println("[BMS] Lote incompleto (timeout) — publico igual con los datos que llegaron.");
    }
    publishTelemetryBMS(bms);

    // --- 3: Temperatura ambiente (DHT22 Sensor) ---
    dht = dhtRead();
    if (dht.valid) {
        lastTempAmb = dht.temperature_c;
    } else {
        Serial.println("[DHT] Lectura invalida — uso el ultimo valor valido.");
    }

    // --- 4: Control térmico ---
    bool pedirShutdown = thermalControlUpdate(millis(), bms.temp_cell_min_c,
                                               bms.temp_cell_max_c, lastTempAmb,
                                               publicarEstadoCooling);
    if (pedirShutdown) {
        Serial.println("[MAIN] Temperatura critica -> pidiendo shutdown al inversor.");
        //##########################################
        // Esto hay que revisarlo bien a ver si anda
        //##########################################
        inverterWrite(REG_SHUTDOWN, 1);
    }

    // --- 5: Inversor (Modbus) + telemetría ---
    // pollModbus hace varias transacciones Modbus bloqueantes, así que se
    // sondea de forma periódica en vez de en cada vuelta del loop.
    if (millis() - lastPollInv >= DEFAULT_POLL_MODBUS_MS) {
        lastPollInv = millis();
        pollModbus(datosInv);

        publishTelemetryInv(datosInv, "StatusData");
        publishTelemetryInv(datosInv, "AcData");
        publishTelemetryInv(datosInv, "DcData");
        publishTelemetryInv(datosInv, "GridData");
#ifdef INVERTER_PROTOCOL_V3
        publishTelemetryInv(datosInv, "LoadData");
#endif
    }

    // --- 6: Inversor (Reiniciar) ---
    if (millis() - lastVerifyInv > DEFAULT_VERIFY_INIT_MS){
        lastVerifyInv = millis();
        verifyAndReinit();
    }
}