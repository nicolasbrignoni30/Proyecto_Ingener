#include <Arduino.h>
#include <SPI.h>
#include <string>

#include "bms_can.h"
#include "bms_parser.h"
#include "telemetria.h"
#include "cooling_control.h"
#include "dht_sensor.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------

struct Intervals {
    uint32_t listen_bms_ms;
};

// Funcionamiento del MCP2515
ModoFuncionamiento modoCan = MODO_NORMAL;

can_frame rx;
BmsData   bms;
DhtData   T_sensor;

unsigned long LastBmsListen = 0;
unsigned long lastPollDht = 0;
bool apagado_termico = false;

Intervals intervals;

// Datos para el frame enviado hacia al bms en el setup().
can_frame tx;
canid_t id = 0x80004200;
__u8 dlc = 8;
__u8 datos[8] = {0,0,0,0,0,0,0,0};


// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void publicar_estado_termico(const char* key, bool bajar_pot) {
    publish_attribute(key, bajar_pot);
};

void parser(can_frame* ptr_msg){
    uint16_t id = (uint16_t)((*ptr_msg).can_id);
    bms_parse_can(id, (*ptr_msg).data, bms);
}

// ---------------------------------------------------------------------------
// Funciones de Inicializacion
// ---------------------------------------------------------------------------

void init_intervals() {
    intervals.listen_bms_ms  = DEFAULT_LISTEN_BMS_MS;
}

void init(){
    SPI.begin();
    // Se inicializan las uart independientes para el inversor y la alarma.
    INVERTER_SERIAL.begin(INVERTER_BAUD, SERIAL_8N1, INVERTER_RX_PIN, INVERTER_TX_PIN);
    
    bmsCanInit(modoCan);
    coolingControlInit();
    dhtInit(DHT_PIN);
    connectWiFi();
    connectMQTT();
}

void init_all_defaults(){
    init_intervals();
    cooling_thresholds_init_defaults();
};

// ---------------------------------------------------------------------------
// finalizan las Funciones de Inicializacion
// ---------------------------------------------------------------------------

void shared_attributes_update(const String& key, float value) {
    cooling_update_threshold(key, value);
    if      (key == "listen_bms_ms")  intervals.listen_bms_ms  = (uint32_t)value;
    else return;
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
    unsigned long t0 = millis();
    while (millis() - t0 < 8000) {
        loopMQTT();   // cada llamada procesa un paquete: primero los SUBACK, después la respuesta
        delay(10);
    }
    
    // Se envia el frame que hace que el bms comience a mandar periodicamente sus paquetes
    bmsSend(id, datos, dlc, &tx);
}

int8_t num_bms_frames = 9;
int16_t batch_timeout = 1500;
int16_t buffer[9];

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
    // Se chequean tanto la conexion wifi como mqtt
    if (!checkWiFiConnection()) connectWiFi();
    if (!checkMQTTConnection()) {
        Serial.print("MQTT desconectado, state: ");
        mqttstate();
        connectMQTT();
    }

    // Se llama periodicamente a loopMQTT para llamar al callback si atributos cambiaron.
    loopMQTT();

    bmsReceiveBatch(&rx, num_bms_frames, batch_timeout, parser);

    // Se reciben los 9 frames que envia el bms se parsea lo importante y se envia a thingsboard
    if (millis() - LastBmsListen > intervals.listen_bms_ms){
        LastBmsListen = millis();
        publishTelemetryBMS(bms);
    }

    T_sensor = dhtRead();
    apagado_termico = coolingControlUpdate(T_sensor.temperature_c, publicar_estado_termico);

    if (millis() - lastPollDht > 3000){
        lastPollDht = millis();
        if (T_sensor.valid) publishTelemetrySensor(T_sensor.temperature_c, T_sensor.humidity_pct);
    }
}