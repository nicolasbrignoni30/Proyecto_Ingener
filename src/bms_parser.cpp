#include "bms_parser.h"

// =============================================================================
// bms_parse_can — Pylontech CAN Bus High Voltage V1.24
// =============================================================================
void bms_parse_can(uint16_t base, const __u8* data, BmsData& bms) {
    // Usamos máscara 0xFFF0 para capturar el ID base 0x4210 independientemente 
    // de la dirección de la batería (1-F) [2]. 
    uint16_t raw_value = 0;

    // Falta el de "temp_fet_c"

    switch (base) {
        case 0x4210:
            // --- Voltaje Total ---
            raw_value = (data[1] << 8) | data[0];
            bms.voltage_v = raw_value * 0.1f; // Resolución 0.1V, Offset 0 [4].

            // --- Corriente ---
            raw_value = (data[3] << 8) | data[2];
            bms.current_a = (raw_value * 0.1f) - 3000.0f; // Resolución 0.1A, Offset -3000A [4].

            // --- Temperatura BMS (Second Level) ---
            // LSB: data[7] bajo, data[8] alto [1].
            raw_value = (data[5] << 8) | data[4];
            bms.temp_avg_c = (raw_value * 0.1f) - 100.0f; // Resolución 0.1°C, Offset -100°C [4].

            // --- SOC y SOH ---
            // Son bytes individuales, resolución 1%, offset 0 [4].
            bms.soc_pct = data[6];
            bms.soh_pct = data[7];
            break;

        case 0x4220:
            // --- Charge Cutoff Voltage ---
            raw_value = (data[1] << 8) | data[0];
            bms.charge_cutoff_v = raw_value * 0.1f;

            // --- Discharge Cutoff Voltage ---
            raw_value = (data[3] << 8) | data[2];
            bms.discharge_cutoff_v = raw_value * 0.1f;

            // --- Max Charge Current ---
            raw_value = (data[5] << 8) | data[4];
            bms.max_charge_a = (raw_value * 0.1f) - 3000.0f;

            // --- Max Discharge Current ---
            raw_value = (data[7] << 8) | data[6];
            bms.max_discharge_a = (raw_value * 0.1f) - 3000.0f;  
            break;

        case 0x4230:
            // --- Max Single Batery Cell Voltage ---
            raw_value = (data[1] << 8) | data[0];
            bms.cell_voltage_max_v = raw_value * 0.001f;

            // --- Min Single Batery Cell Voltage ---
            raw_value = (data[3] << 8) | data[2];
            bms.cell_voltage_min_v = raw_value * 0.001f;
            break;

        case 0x4240:
            // --- Max Single Battery Cell Voltage ---
            raw_value = (data[1] << 8) | data[0];
            bms.cell_voltage_max_v = (raw_value * 0.1f) - 100.0f;

            // --- Min Single Battery Cell Voltage ---
            raw_value = (data[3] << 8) | data[2];
            bms.cell_voltage_min_v = (raw_value * 0.1f) - 100.0f;   
            break;

        case 0x4250:
            // --- Error ---
            bms.fault = data[3];
            bms.alarm = (data[5] << 8) | data[4];
            bms.protection = (data[7] << 8) | data[6];

            bms.force_charge_req = ((data[0] >> 3) & 1) == 1;
            bms.charging = ((data[0] & 0x07) == 1);
            bms.discharging = ((data[0] & 0x07) == 2);
            break;

        case 0x4280:
            bms.charge_forbidden = (data[0] == 0xAA);
            bms.soe_pct = data[3];
            break;

        default:
            break;
    }
}