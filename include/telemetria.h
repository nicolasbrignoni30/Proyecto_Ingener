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

// Interfaz pública del módulo
bool checkWiFiConnection();
void connectWiFi();
bool checkMQTTConnection();
void connectMQTT();
void loopMQTT();
void publishTelemetryBMS(const BmsData& datosBms);
void publishTelemetryInv(const InvData& inv, const std::string& campo);
void updateSim(Sim& sim);
void publishTelemetrySim(const Sim& data);

#endif