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
#include "inverter.h"
#include "telemetria.h"
#include "thermal_control.h"
#include "dht_sensor.h"
#include "gas_alarm.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------

struct Intervals {
    uint32_t listen_bms_ms;
    uint32_t poll_modbus_ms;
    uint32_t verify_init_ms;
    uint32_t poll_gas_alarm_ms;
};

// Funcionamiento del MCP2515
ModoFuncionamiento modoCan = MODO_NORMAL;

can_frame canMsgRx;
BmsData   bms;
DhtData   dht;
InvData   datosInv;
DhtData   T_sensor;
GasData   G_alarm;

unsigned long lastPollInv = 0;
unsigned long lastVerifyInv = 0;
unsigned long LastBmsListen = 0;
unsigned long lastPollGasAlarm = 0;
unsigned long lastReconexion = 0;
unsigned long lastCicloInv = 0;
unsigned long lastPollDht = 0;

Intervals intervals;

// Datos para el frame enviado hacia al bms en el setup().
can_frame tx;
canid_t id = 0x80004200;
__u8 dlc = 8;
__u8 datos[8] = {0,0,0,0,0,0,0,0};


// ---------------------------------------------------------------------------
// Variables (atributos compartidas) definidas en el Main
// ---------------------------------------------------------------------------

float limit_power;
float I_MAX;
float I_MIN;
float V1;
float V2;         
bool vehiculo_conectado = false; 
bool off_grid_ack = false; 

// ---------------------------------------------------------------------------
// Atributos de Cliente
// ---------------------------------------------------------------------------

bool grid_ok = false;
bool inv_ok  = false;
bool power_off = false;

// ---------------------------------------------------------------------------
// Booleanos que no son variables compartidas
// ---------------------------------------------------------------------------

bool apagado_termico = false;   // lo pide thermal_control
bool grid_ok_ant = false;

// ---------------------------------------------------------------------------
// Control del inversor
// ---------------------------------------------------------------------------

// Lee el reg 32 y publica grid_ok si cambió. Devuelve false si el inversor no respondió.
// Si mas de cinco veces seguidas no se puedo leer el inversor se pasan los booleanos a false
bool leerEstadoInversor() {
    static uint8_t fallas = 0;
    static int8_t  grid_publicado = -1;
    static int8_t  inv_publicado  = -1;
    int16_t reg_32;

    bool respondio = inverterRead(REG_STATUS, REG_STATUS_COUNT, &reg_32);

    if (respondio) {
        fallas  = 0;
        grid_ok = (reg_32 & 0x0008) && !(reg_32 & 0x0010);   // b3 = con red, b4 = isla
        inv_ok  = (reg_32 & 0x0004) && !(reg_32 & 0x0001);   // b2 = encendido, b0 = falla
    } else {
        if (fallas < 5) fallas++;
        if (fallas == 5) {               // 5 seguidas sin respuesta: se asume que no hay red
            grid_ok = false;
            inv_ok  = false;
        }
    }

    // Se publica si respondió o si ya van 5 fallas. Una falla suelta no se publica.
    if (respondio || fallas == 5) {
        if (grid_ok != grid_publicado && publish_attribute("grid_ok", grid_ok)) grid_publicado = grid_ok;
        if (inv_ok  != inv_publicado  && publish_attribute("inv_ok",  inv_ok))  inv_publicado  = inv_ok;
    }

    return respondio;   // true solo si el inversor contestó: el loop escribe solo en ese caso
}


// Escribe lo mismo que el EMS original: todo cada 1 s y 329/334/351 cada 10 s.
void escribirInversor() {
    static uint8_t ciclo   = 0;
    static float I_chg = 0;

    power_off = apagado_termico; //En un futuro esto podria ser un OR con otras condiciones de apagado que salgan de otro lugar que no sea el control termico.

    if (bms.soc_pct == 100){ // Si llego al 100% se detiene la carga
        I_chg = 0.0;
    }else{
        if (bms.cell_voltage_max_v <= V1){
            I_chg = I_MAX;
        }
        else if (bms.cell_voltage_max_v <= V2){
            I_chg = max(I_MIN, I_MAX * ((V2 - bms.cell_voltage_max_v) / (V2 - V1)));
        }
            else if (bms.cell_voltage_max_v > V2){
                I_chg = I_MIN;
            }
    }

    // Cada 10 s (y en el primer ciclo). Sin 329/334 el inversor no acepta los 6000.
    if (ciclo == 0) {
        inverterWrite(REG_BMS_MASTER_SLAVE, 1);
        inverterWrite(REG_BMS_POWER_SCHED,  1);
        inverterWrite(REG_OFFGRID_PWR_REG,  2);
    }
    if (++ciclo >= 10) ciclo = 0;

    float setpoint_kw = (inv_ok && grid_ok) ? -limit_power : 0.0f;

    // Cada 1 s, en el mismo orden que el EMS
    if (inverterWrite(apagado_termico ? REG_SHUTDOWN : REG_POWER_ON, 1)) publish_attribute("power_off", power_off);
    inverterWrite(BMS_BATTERY_SOC,           bms.soc_pct / SCALE_SOC_SOH);
    inverterWrite(BMS_MAX_CHG_CURRENT,       (int16_t)(bms.max_charge_a / SCALE_CURRENT_A));
    inverterWrite(BMS_MAX_DISCHG_CURRENT,    (int16_t)(bms.max_discharge_a / SCALE_CURRENT_A));
    inverterWrite(REG_DC_MAX_CHG_CURRENT,    (int16_t)(I_chg / SCALE_CURRENT_A));
    inverterWrite(REG_DC_MAX_DISCHG_CURRENT, bms.soc_pct < SOC_RESERVA ? 0 : (int16_t)(inverter_values.dc_max_dischg_current / SCALE_CURRENT_A));
    inverterWrite(REG_3PHASE_CTRL_MODE,      inverter_values.three_phase_ctrl_mode_value);
    inverterWrite(REG_PV_SWITCH,             inverter_values.pv_switch_value);
    inverterWrite(REG_ANTI_BACKFLOW,         inverter_values.anti_backflow_value);
    inverterWrite(REG_LEAKAGE_DETECT,        inverter_values.leakage_detect_value);
    inverterWrite(REG_DCDC_SWITCH,           inverter_values.dcdc_switch_value);
    inverterWrite(REG_SET_POWER,             (int16_t)(setpoint_kw /  SCALE_SET_POWER_KW));
}

// Máquina de estados
void actualizarModo() {
    if (grid_ok){
        off_grid_ack = false;
        if (!grid_ok_ant) grid_ok_ant = inverterWrite(REG_GRID_CONNECT_CMD, 1) ? true : false;
    }else{
        grid_ok_ant = false;
        if (off_grid_ack){ // esto es para saber si thingsboard se entero de que no hay red y bajo la potencia
            inverterWrite(REG_OFFGRID_CMD, 1);
        }
    }
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

// thermal_control llama a esto para reportar reducción de potencia / shutdown.
// Publica a thingsboard dos booleanos para poder hacer la logica
// de si hay que avisarle a EveMove que hay que bajar la potencia o directamente cortar
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

void init_defaults_main() {
    intervals.listen_bms_ms  = DEFAULT_LISTEN_BMS_MS;
    intervals.poll_modbus_ms = DEFAULT_POLL_MODBUS_MS;
    intervals.verify_init_ms = DEFAULT_VERIFY_INIT_MS;
    intervals.poll_gas_alarm_ms = DEFAULT_GAS_ALARM_MS;
    limit_power = DEFAULT_LIMIT_POWER;
    I_MAX = DEFAULT_I_MAX;
    I_MIN = DEFAULT_I_MIN;
    V1 = DEFAULT_V1;
    V2 = DEFAULT_V2;
}

void init(){
    SPI.begin();
    // Se inicializan las uart independientes para el inversor y la alarma.
    INVERTER_SERIAL.begin(INVERTER_BAUD, SERIAL_8N1, INVERTER_RX_PIN, INVERTER_TX_PIN);
    GAS_SERIAL.begin(GAS_BAUD, SERIAL_8N1, GAS_RX_PIN, GAS_TX_PIN);

    inverterInit(INVERTER_SERIAL, INVERTER_DE_RE_PIN);
    bmsCanInit(modoCan);
    dhtInit(DHT_PIN);
    gasAlarmInit(GAS_SERIAL, GAS_DE_RE_PIN);
    connectWiFi();
    connectMQTT();
}

void init_all_defaults(){
    inverter_init_defaults();
    thermal_thresholds_init_defaults();
    init_defaults_main();
};

// ---------------------------------------------------------------------------
// finalizan las Funciones de Inicializacion
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Funcion de telemetria auxiliar para el inversor
// ---------------------------------------------------------------------------

void publicarTelemetriaInv() {
    publishTelemetryInv(datosInv, "StatusData");
    publishTelemetryInv(datosInv, "AcData");
    publishTelemetryInv(datosInv, "DcData");
    publishTelemetryInv(datosInv, "GridData");
    //publishTelemetryInv(datosInv, "LoadData"); //Esto da problemas pues JSON envia null
    publishTelemetryInv(datosInv, "BMSData");
}


void shared_attributes_update(const String& key, float value) {
    inverter_update_reg_values(key, value);
    thermal_update_threshold(key, value);

    if (key == "limit_power")  limit_power  = value;
    else if (key == "I_MAX") I_MAX = value;
    else if (key == "I_MIN") I_MIN = value;
    else if (key == "V1") V1 = value;
    else if (key == "V2") V2 = value;
    else if (key == "vehiculo_conectado") vehiculo_conectado = value;
    else if (key == "off_grid_ack") off_grid_ack = value;
    
    // --- intervalos de tiempo ---
    else if (key == "listen_bms_ms")  intervals.listen_bms_ms  = (uint32_t)value;
    else if (key == "poll_modbus_ms") intervals.poll_modbus_ms = (uint32_t)value;
    else if (key == "verify_init_ms") intervals.verify_init_ms = (uint32_t)value;
    else if (key == "poll_gas_alarm_ms") intervals.poll_gas_alarm_ms = (uint32_t)value;
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
    delay(8000); // Este pequeño delay es para que lleguen bien las cosas

    // Se llama a loopMQTT()
    loopMQTT();

    // A diferencia de con thermal_control, para el inversor hay que volver a escribir los registros
    inverter_reinit_from_cloud();
  
    // Se envia el frame que hace que el bms comience a mandar periodicamente sus paquetes
    bmsSend(id, datos, dlc, &tx);
}

int8_t num_bms_frames = 9;
int16_t batch_timeout = 1500;
int16_t reg_32 = 0;


// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
    // Se chequean tanto la conexion wifi como mqtt
    if (!checkMQTTConnection() && checkWiFiConnection() && millis() - lastReconexion > 30000) {
        lastReconexion = millis();
        Serial.print("MQTT desconectado, state: ");
        mqttstate();
        connectMQTT();          // se suscribe adentro
        request_attributes();   // por si cambió algún atributo mientras estaba desconectado
    }

    // Se llama periodicamente a loopMQTT para llamar al callback si atributos cambiaron.
    loopMQTT();

    // Se reciben los datos del bms 
    bmsReceiveBatch(&canMsgRx, num_bms_frames, batch_timeout, parser);

    // Inversor cada 1 s: leer estado -> máquina de estados -> escribir
    if (millis() - lastCicloInv >= 1000) {
        lastCicloInv = millis();
        if (leerEstadoInversor()) {      // si no respondió, ese segundo no se escribe nada
            actualizarModo();
            escribirInversor();
        }
    }

    // Se reciben los 9 frames que envia el bms se parsea lo importante y se envia a thingsboard
    if (millis() - LastBmsListen > intervals.listen_bms_ms){
        LastBmsListen = millis();
        publishTelemetryBMS(bms);
        // Se llama a la funcion de pasamanos para mandarle al inversor los datos del bms (registros 6000s)
    }

    // Se hace un poll al inversor para obtener datos
    if (millis() - lastPollInv > intervals.poll_modbus_ms){
        lastPollInv = millis();
        pollModbus(datosInv);
        publicarTelemetriaInv();
    }

    // Se lee la alarma de gas
    if (millis() - lastPollGasAlarm > intervals.poll_gas_alarm_ms){
        lastPollGasAlarm  = millis();
        gasAlarmReadAll(G_alarm);

        // Se imprimen los valores del sensor de alarma, si todo marcha, se pueden mandar por telemetria
        Serial.println("----------- Valores gas alarm --------------");
        Serial.println(G_alarm.alarm1_point);
        Serial.println(G_alarm.alarm2_point);
        Serial.println(G_alarm.range);
        Serial.println(G_alarm.resolution);
        Serial.println(G_alarm.unit);
        Serial.println(G_alarm.gas_type);
        Serial.println(G_alarm.concentration);
        Serial.println(G_alarm.alarm_status);
    }

    //-------------------------------------------------
    // Se leen los datos del sensor + Control Termico
    //-------------------------------------------------

    if (millis() - lastPollDht > 3000){
        lastPollDht = millis();
        T_sensor = dhtRead();
        if (T_sensor.valid) publishTelemetrySensor(T_sensor.temperature_c, T_sensor.humidity_pct);
        apagado_termico = thermalControlUpdate(millis(), bms.temp_cell_min_c, bms.temp_cell_max_c, T_sensor.temperature_c, publicar_estado_termico);
    }
}