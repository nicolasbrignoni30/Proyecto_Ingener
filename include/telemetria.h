#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h> // Necesario para tipos estándar como String, uint32_t, etc.
#include "bms_parser.h" // Se incluye para poder conocer el tipo BmsData
#include "inverter_parser.h"

// ---------------------------------------------------------------------------
// Intervals
// ---------------------------------------------------------------------------
#define PUBLISH_INTERVAL_BMS  10000
#define PUBLISH_INTERVAL_INV 5000

#define TOPIC_TELEMETRY          "v1/devices/me/telemetry"
#define TOPIC_ATTRIBUTES_SUB     "v1/devices/me/attributes"
#define TOPIC_ATTR_REQUEST       "v1/devices/me/attributes/request/1"
#define TOPIC_ATTR_RESPONSE      "v1/devices/me/attributes/response/1"

#define SHARED_KEYS_REQUEST "{\"sharedKeys\":\"heat_enter_c,heat_exit_c,plate_max_c,plate_min_c,cool_on_c,cool_off_c,cool_pot_c,cool_critical_c, dc_max_dischg_current, dc_max_chg_current, anti_backflow_value, grid_sched_mode_value, three_phase_ctrl_mode_value, pv_switch_value, leakage_detect_value, dcdc_switch_value, set_power, power_on_value\"}"

// Estructura de datos para el caso de datos simulados
struct Sim {
    // Inverter
    float soc;
    float p_inv;
    float grid_p;
    float load_p;
    float freq;
    float v_phase;
    float pf;
    // BMS — LWS Modbus fields
    float bms_v;
    float bms_i;    // positive = charging
    float bms_temp_avg;
    float bms_temp_max;
    float bms_temp_min;
    float bms_temp_fet;
    float bms_cell_v_max;
    float bms_cell_v_min;
    float bms_max_chg_a;
    float bms_max_dischg_a;
    float bms_chg_cutoff;
    float bms_dischg_cutoff;
};

void telemetria_set_attribute_handler(const String& key, float value);

// Interfaz pública del módulo
bool checkWiFiConnection();
void connectWiFi();
bool checkMQTTConnection();
void connectMQTT();
void pedirAtributos();
void suscribirAtributos();
void loopMQTT();
void publishTelemetryBMS(const BmsData& datosBms);
void publishTelemetryInv(const InvData& inv, const std::string& campo);
void publishTemperature(const float temp, bool bajar_pot, bool shut_down);
void updateSim(Sim& sim);
void publishTelemetrySim(const Sim& data);

#endif