#pragma once

#include "bms_can.h"
#include <stdint.h>
#include <stdbool.h>

struct BmsData {
    // — Electrical —
    float    voltage_v;           // total pack voltage (V)
    float    current_a;           // current (A, positive=charging, negative=discharging)
    float    max_charge_a;        // max allowed charge current (A)
    float    max_discharge_a;     // max allowed discharge current (A)
    float    charge_cutoff_v;     // charge cutoff voltage (V)   — overcharge protection on
    float    discharge_cutoff_v;  // discharge cutoff voltage (V) — overdischarge protection on

    // — State —
    float    soc_pct;             // State of Charge (%)
    float    soh_pct;             // State of Health (%)

    // — Temperature —
    float    temp_avg_c;          // average cell temperature (°C)
    float    temp_cell_max_c;     // max cell temperature (°C)
    float    temp_cell_min_c;     // min cell temperature (°C)
    float    temp_fet_c;          // FET temperature (°C)

    // — Cell voltages —
    float    cell_voltage_max_v;  // max cell voltage (V)
    float    cell_voltage_min_v;  // min cell voltage (V)

    // — Status flags —
    bool     charging;            // BMS in charging state
    bool     discharging;         // BMS in discharging state
    bool     charge_forbidden;    // charge MOSFET off → charging forbidden
    bool     discharge_forbidden; // discharge MOSFET off → discharging forbidden
    bool     force_charge_req;    // low SOC alarm → force charge request

    // — Diagnostics —
    uint8_t  fault;               // fault byte
    uint16_t alarm;               // alarm word
    uint16_t protection;          // protection word
    uint8_t  soe_pct;             // State of Energy % (Pylontech CAN only, 0 if unavailable)

    bool     valid;               // true once successfully parsed
};


// Parse a single Pylontech CAN frame into BmsData.
// Call for each received frame — BmsData.valid is set when 0x4210 is received.
// canId: the full 29-bit extended CAN ID
// data:  8 bytes of frame data
void bms_parse_can(uint16_t base, const __u8* data, BmsData& bms);