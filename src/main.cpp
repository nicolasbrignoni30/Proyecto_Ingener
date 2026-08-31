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
};

void init(){
    SPI.begin();
    Serial2.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    inverterInit(RS485_SERIAL, RS485_DE_RE_PIN);
    bmsCanInit(modoCan);
    dhtInit(DHT_PIN);
    connectWiFi();
    connectMQTT();
};

void init_all_defaults(){
    inverter_init_defaults();
    thermal_thresholds_init_defaults();
};


void telemetria_set_attribute_handler(const String& key, float value) {
    thermal_update_threshold(key, value);
    inverter_update_reg_values(key, value);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    while (!Serial); // espera a que se abra la terminal (útil en algunas placas)

    // Se inicializan todos lo modulo.
    init();

    // Se incializan todos los valores default puesto en la memoria flash.
    init_all_defaults();

    // Se piden los atributos 
    pedirAtributos();
    delay(10000);

    // Se llama a loopMQTT()
    loopMQTT();

    Serial.println("[MAIN] Setup finalizado con exito. Corriendo lazo...");
    Serial.println("--------------------------------------------------");

    // A diferencia de con thermal_control, para el inversor hay que volver a escribir los registros
    //inverter_reinit_from_cloud();

    Serial.println("--- Thermal Thresholds ---");
    Serial.print("heat_enter_c: ");    Serial.println(thermal_thresholds.heat_enter_c);
    Serial.print("heat_exit_c: ");     Serial.println(thermal_thresholds.heat_exit_c);
    Serial.print("plate_max_c: ");     Serial.println(thermal_thresholds.plate_max_c);
    Serial.print("plate_min_c: ");     Serial.println(thermal_thresholds.plate_min_c);
    Serial.print("cool_on_c: ");       Serial.println(thermal_thresholds.cool_on_c);
    Serial.print("cool_off_c: ");      Serial.println(thermal_thresholds.cool_off_c);
    Serial.print("cool_pot_c: ");      Serial.println(thermal_thresholds.cool_pot_c);
    Serial.print("cool_critical_c: "); Serial.println(thermal_thresholds.cool_critical_c);

    Serial.println("--- Inverter Values ---");
    Serial.print("dc_max_dischg_current: ");       Serial.println(inverter_values.dc_max_dischg_current);
    Serial.print("dc_max_chg_current: ");          Serial.println(inverter_values.dc_max_chg_current);
    Serial.print("anti_backflow_value: ");         Serial.println(inverter_values.anti_backflow_value);
    Serial.print("grid_sched_mode_value: ");       Serial.println(inverter_values.grid_sched_mode_value);
    Serial.print("three_phase_ctrl_mode_value: "); Serial.println(inverter_values.three_phase_ctrl_mode_value);
    Serial.print("pv_switch_value: ");             Serial.println(inverter_values.pv_switch_value);
    Serial.print("leakage_detect_value: ");        Serial.println(inverter_values.leakage_detect_value);
    Serial.print("dcdc_switch_value: ");           Serial.println(inverter_values.dcdc_switch_value);
    Serial.print("set_power: ");                   Serial.println(inverter_values.set_power);
    Serial.print("power_on_value: ");               Serial.println(inverter_values.power_on_value);
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {}