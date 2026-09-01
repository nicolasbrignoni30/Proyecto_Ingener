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
DhtData d;

unsigned long lastPollInv = 0;
unsigned long lastVerifyInv = 0;
unsigned long LastBmsListen = 0;

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
// Publica a thingsboard dos booleanos para poder hacer la logica
// de si hay que avisarle a EveMove que hay que bajar la potencia o directamente cortar
void publicarEstadoCooling(bool bajar_pot, bool shut_down) {
    publishCoolingAttributes(bajar_pot, shut_down);
};

void parser(can_frame* ptr_msg){
    uint16_t id = (uint16_t)((*ptr_msg).can_id);
    bms_parse_can(id, (*ptr_msg).data, bms);
}

// ---------------------------------------------------------------------------
// Aqui terminan los callbacks
// ---------------------------------------------------------------------------


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

void publicarTelemetriaInv() {
    publishTelemetryInv(datosInv, "StatusData");
    publishTelemetryInv(datosInv, "AcData");
    publishTelemetryInv(datosInv, "DcData");
    publishTelemetryInv(datosInv, "GridData");
    publishTelemetryInv(datosInv, "LoadData");
}


void telemetria_set_attribute_handler1(const String& key, float value) {
    thermal_update_threshold(key, value);
    inverter_update_reg_values(key, value);
}

void telemetria_set_attribute_handler2(const String& key, float value) {
    thermal_update_threshold(key, value);
    inverter_queue_write(key, value);
}


// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    while (!Serial);

    // Se inicializan todos lo modulo.
    init();

    // Se incializan todos los valores default puesto en la memoria flash.
    init_all_defaults();

    // Se piden los atributos 
    request_attributes();
    delay(6000); // Este pequeño delay es para que lleguen bien las cosas

    // Se llama a loopMQTT()
    loopMQTT();

    Serial.println("[MAIN] Setup finalizado con exito. Corriendo lazo...");
    Serial.println("--------------------------------------------------");

    // A diferencia de con thermal_control, para el inversor hay que volver a escribir los registros
    inverter_reinit_from_cloud();

    // Se modifica el Callback para adecuarlo a cuando cambian algunos atributos.
    setCallback();
}

int8_t num_bms_frames = 9;
int16_t batch_timeout = 1500;


// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {

    // Se llama periodicamente a loopMQTT para llamar al callback si atributos cambiaron.
    loopMQTT();

    // Se envian hacia el inversor los registros que cambiaron y guardados en la cola
    inverter_process_pending_writes();

    // Se reciben los 9 frames que envia el bms se parsea lo importante y se envia a thingsboard
    if (millis() - LastBmsListen > DEFAULT_LISTEN_BMS_MS){
        LastBmsListen = millis();
        bmsReceiveBatch(&canMsgRx, num_bms_frames, batch_timeout, parser);
        publishTelemetryBMS(bms);
    }

    // Se hace un poll al inversor para obtener datos
    if (millis() - lastPollInv > DEFAULT_POLL_MODBUS_MS){
        lastPollInv = millis();
        pollModbus(datosInv);
        publicarTelemetriaInv();
    }

    // Se leen los datos del sensor
    d = dhtRead();

    if (thermalControlUpdate(millis(), bms.temp_cell_min_c, bms.temp_cell_max_c, d.temperature_c, publicarEstadoCooling)){
        inverterWrite(REG_SHUTDOWN, 1);
    }

}