#pragma once
#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// inverter_parser — pure register decoding for SinoSoar SP6030
// No Arduino/ESP32 dependencies — safe to include in host unit tests.
// Scale factors from: SinoSoar PCS Modbus Communication Protocol V3.0
// =============================================================================

// --- Register addresses (from protocol table) --------------------------------
#define REG_DC_MAX_DISCHG_CURRENT  763
#define REG_DC_MAX_CHG_CURRENT     764
#define REG_3PHASE_CTRL_MODE       341
#define REG_PV_SWITCH              652
#define REG_LEAKAGE_DETECT         795
#define REG_DCDC_SWITCH            656
#define REG_POWER_ON               650
#define REG_FUNCTION_MGMT          873   // bit0=anti-backflow/self-use. EMS oficial usa 873=1 + reg 353
#define REG_ANTI_BACKFLOW          873   // alias
#define REG_GRID_SCHED_MODE        758   // 0=AC constant power. Necesario para reg 135 (estrategia B)
#define REG_SET_POWER              353   // active power setpoint. +kW=discharge, -kW=charge. precision 0.1kW. Estrategia B

// Decoded AC measurements (registers 100–125)
struct AcData {
    float freq_hz;
    float v_ab, v_bc, v_ca;       // line voltages (V)
    float i_a,  i_b,  i_c;        // phase currents (A)
    float v_a,  v_b,  v_c;        // phase voltages (V)
    float p_a,  p_b,  p_c;        // active power per phase (kW)
    float q_a,  q_b,  q_c;        // reactive power per phase (kvar)
    float pf_a, pf_b, pf_c;       // power factor per phase
    float p_inv;                   // total active power (kW)
    float q_inv;                   // total reactive power (kvar)
    float pf_total;                // total power factor
};

// Decoded DC measurements (registers 141–143)
struct DcData {
    float power_kw;
    float voltage_v;
    float current_a;
};

// Decoded grid measurements (registers 170–192)
struct GridData {
    float freq_hz;
    float v_a, v_b, v_c;          // phase voltages (V)
    float p_kw;                    // total active power (kW)
};

// Decoded load measurements (registers 200–213, protocol V3.0+)
struct LoadData {
    float freq_hz;
    float i_a,  i_b,  i_c;
    float v_a,  v_b,  v_c;
    float p_a,  p_b,  p_c;        // per-phase active power (kW)
    float p_total;                 // total active power (kW)
    float s_total;                 // total apparent power (kVA)
};

// Status register (reg 32) bit fields
struct StatusData {
    bool fault;
    bool alarm;
    bool running;
    bool grid_tied;
    bool off_grid;
    bool derating;
    bool standby;
};

struct FirmData {
    uint32_t fw_hw_version;
    uint32_t fw_dsp_version;
    uint32_t fw_com_version;
    uint32_t fw_rtu_protocol;
};

// Unificación por composición en inverter_parser.h
struct InvData {
    StatusData status;
    AcData     ac;
    DcData     dc;
    GridData   grid;
    LoadData   load;
    FirmData   firm;
};


// Parse raw registers from readRegisters() into typed structs.
// raw[] must have at least the count passed to readRegisters().
void inverter_parse_ac    (const int16_t* raw, AcData&     out);
void inverter_parse_dc    (const int16_t* raw, DcData&     out);
void inverter_parse_grid  (const int16_t* raw, int16_t grid_p_raw, GridData& out);
void inverter_parse_load  (const int16_t* raw, LoadData&   out);
void inverter_parse_status(const int16_t* raw, StatusData& out);
