// =============================================================================
// inverter.cpp — Capa de hardware para inversor SinoSoar SP6030
//
// Instancia propia de ModbusMaster (FC03 lectura, FC06 escritura).
// Protocolo: SinoSoar PCS Modbus V3.0
// Baud: 115200, device ID configurable en config.h (MODBUS_DEVICE_ID)
//
// API pública:
//   inverterInit(serial, deRePin) — init Modbus y secuencia de arranque
//   pollModbus(telemetry)         — lee todos los bloques de registros
//   inverterSetPower(kw)          — escribe setpoint AC (reg 135, 0.1kW)
//   inverterPowerOn()             — enciende inversor (reg 650 = 1)
//   inverterShutdown()            — apaga inversor    (reg 650 = 0)
//   verifyAndReinit()             — verifica y corrige registros de config
//   readFirmwareVersion(mqtt)     — lee versión y publica como atributos TB
// =============================================================================

#include "inverter.h"
#include "inverter_parser.h"
#include "inverter_scales.h"
#include "config.h"
#include <ModbusMaster.h>

// ---------------------------------------------------------------------------
// Modbus node — inverter
// ---------------------------------------------------------------------------
static ModbusMaster inv;

inverterValues inverter_values;

namespace{

    //Estas funciones se pasan como parametro en la inicializacion.
    //De esta manera la biblioteca de modbusmaster saber que hacer antes y despues de transmitir.

    static uint8_t _deRePin;

    static void preTransmission()  { digitalWrite(_deRePin, HIGH); }
    static void postTransmission() { digitalWrite(_deRePin, LOW);  }

    static const char* modbusErrorStr(uint8_t code) {
        switch (code) {
            case 0x01: return "illegal function";
            case 0x02: return "illegal data address";
            case 0x03: return "illegal data value";
            case 0x04: return "slave device failure";
            case 0xE0: return "invalid slave ID";
            case 0xE1: return "invalid function";
            case 0xE2: return "response timed out";
            case 0xE3: return "invalid CRC";
            default:   return "unknown error";
        }
    }

    InitCmd seq[CANT_REG_INIT];

    const InitCmd* inverter_init_sequence(uint8_t* count_out) {
        seq[0] = { REG_DC_MAX_DISCHG_CURRENT, ((int16_t)(inverter_values.dc_max_dischg_current / SCALE_CURRENT_A)), "Max DC discharge = 100A" };
        seq[1] = { REG_DC_MAX_CHG_CURRENT,    ((int16_t)(inverter_values.dc_max_chg_current / SCALE_CURRENT_A)),    "Max DC charge = 20" };
        seq[2] = { REG_ANTI_BACKFLOW,         inverter_values.anti_backflow_value,                                  "Self-use OFF (on-grid mode)" };
        seq[3] = { REG_GRID_SCHED_MODE,       inverter_values.grid_sched_mode_value,                                "AC side constant power (reg 758)" };
        seq[4] = { REG_3PHASE_CTRL_MODE,      inverter_values.three_phase_ctrl_mode_value,                          "3-phase independent control" };
        seq[5] = { REG_PV_SWITCH,             inverter_values.pv_switch_value,                                      "PV OFF" };
        seq[6] = { REG_LEAKAGE_DETECT,        inverter_values.leakage_detect_value,                                 "Leakage detect OFF" };
        seq[7] = { REG_DCDC_SWITCH,           inverter_values.dcdc_switch_value,                                    "DCDC OFF" };
        seq[8] = { REG_SET_POWER,             ((int16_t)(inverter_values.set_power / SCALE_SET_POWER_KW)),          "Setpoint = -- kW" };
        seq[9] = { REG_POWER_ON,              inverter_values.power_on_value,                                       "Power ON" };

        *count_out = sizeof(seq) / sizeof(seq[0]); //Manera rara de contar la cantidad de struct InitCmd en seq.
        return seq;
    }

    bool inverter_run_init() {
        uint8_t count;
        const InitCmd* cfg = inverter_init_sequence(&count);
        bool ok = true;
        for (uint8_t i = 0; i < count; i++)
            ok &= inverterWrite(cfg[i].reg, cfg[i].val);
        return ok;
    }

    void asignar_valores_default(){
        inverter_values.dc_max_dischg_current       = DEFAULT_DC_MAX_DISCHG_CURRENT;
        inverter_values.dc_max_chg_current          = DEFAULT_DC_MAX_CHG_CURRENT;
        inverter_values.anti_backflow_value         = DEFAULT_ANTI_BACKFLOW_VALUE;
        inverter_values.grid_sched_mode_value       = DEFAULT_SCHED_MODE_VALUE;
        inverter_values.three_phase_ctrl_mode_value = DEFAULT_THREE_PHASE_CTRL_MODE_VALUE;
        inverter_values.pv_switch_value             = DEFAULT_PV_SWITCH_VALUE;
        inverter_values.leakage_detect_value        = DEFAULT_LEAKAGE_DETECT_VALUE;
        inverter_values.dcdc_switch_value           = DEFAULT_DCDC_SWITCH_VALUE;
        inverter_values.set_power                   = DEFAULT_SET_POWER;
        inverter_values.power_on_value              = DEFAULT_POWER_ON_VALUE;
    }

    void asignar_valores_cloud(const inverterValues& nuevos) {
    inverter_values = nuevos;
    }

};


// ---------------------------------------------------------------------------
// Lectura y Escritura de Registros.
// ---------------------------------------------------------------------------

bool inverterRead(uint16_t reg, uint16_t count, int16_t* out) {
    uint8_t result = inv.readHoldingRegisters(reg, count);
    if (result != ModbusMaster::ku8MBSuccess) {
        Serial.printf("[Inverter] Read reg %d count %d failed: 0x%02X — %s\n",
                      reg, count, result, modbusErrorStr(result));
        return false;
    }
    for (uint16_t i = 0; i < count; i++)
        out[i] = (int16_t)inv.getResponseBuffer(i);
    return true;
}


bool inverterWrite(uint16_t reg, int16_t value) {
    uint8_t result = inv.writeSingleRegister(reg, (uint16_t)value);
    if (result != ModbusMaster::ku8MBSuccess) {
        Serial.printf("[Inverter] Write reg %d = %d failed: 0x%02X — %s\n",
                      reg, value, result, modbusErrorStr(result));
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void inverter_init_defaults(HardwareSerial& serial, uint8_t deRePin) {
    _deRePin = deRePin;
    pinMode(deRePin, OUTPUT);
    digitalWrite(deRePin, LOW);
    inv.begin(MODBUS_DEVICE_ID, serial);
    inv.preTransmission(preTransmission);
    inv.postTransmission(postTransmission);
    
    // Se ponen los valores default en la variable inverter_values compartida
    asignar_valores_default();

    Serial.println("[Inverter] Configurando...");
    bool ok = inverter_run_init();
    Serial.printf("[Inverter] Init %s\n", ok ? "OK" : "WARNING: algún registro falló");
}

void inverter_reinit_from_cloud(const inverterValues& inv_values){
    asignar_valores_cloud(inv_values);

    bool ok = inverter_run_init();
    Serial.printf("[Inverter] Init %s\n", ok ? "OK" : "WARNING: algún registro falló");
};

// ---------------------------------------------------------------------------
// Verify and reinit
// ---------------------------------------------------------------------------
void verifyAndReinit() {
    uint8_t count;
    const InitCmd* cfg = inverter_init_sequence(&count); 

    bool anyFixed = false;
    for (uint8_t i = 0; i < count; i++) {
        int16_t cur;
        if (!inverterRead(cfg[i].reg, 1, &cur)) {
            Serial.printf("[Verify] Error reg %d\n", cfg[i].reg);
            continue;
        }
        if (cur != cfg[i].val) {
            Serial.printf("[Verify] reg %d: %d → %d\n", cfg[i].reg, cur, cfg[i].val);
            inverterWrite(cfg[i].reg, cfg[i].val);
            anyFixed = true;
            delay(100);
        }
    }
    Serial.println(anyFixed ? "[Verify] Parámetros corregidos" : "[Verify] Config OK");
}

// ---------------------------------------------------------------------------
// Firmware version
// ---------------------------------------------------------------------------
void readFirmwareVersion(FirmData& firm) {
    int16_t v[REG_VERSION_COUNT];
    if (!inverterRead(REG_VERSION_START, REG_VERSION_COUNT, v)) {
        Serial.println("[Inverter] Error al leer versión");
        return;
    }
    uint32_t hw_ver  = ((uint32_t)(uint16_t)v[12] << 16) | (uint16_t)v[13];
    uint32_t dsp_fw  = ((uint32_t)(uint16_t)v[14] << 16) | (uint16_t)v[15];
    uint32_t com_fw  = ((uint32_t)(uint16_t)v[17] << 16) | (uint16_t)v[18];
    uint16_t rtu_ver = (uint16_t)v[19];

    Serial.printf("[Inverter] Model: %d  HW: %08X  DSP: %08X  COM: %08X  RTU: %d\n",
                  v[1], hw_ver, dsp_fw, com_fw, rtu_ver);
    if (rtu_ver >= 30)
        Serial.println("[Inverter] ✓ Protocolo V3.0+");
    else
        Serial.printf("[Inverter] ⚠ Protocolo V%d — regs 200-213 pueden no estar\n", rtu_ver);

    firm.fw_hw_version = hw_ver;
    firm.fw_dsp_version = dsp_fw;
    firm.fw_com_version = com_fw;
    firm.fw_rtu_protocol = rtu_ver;
}


// ---------------------------------------------------------------------------
// Set power
// ---------------------------------------------------------------------------
bool inverterSetPower(float kw) {
    int16_t raw = (int16_t)(kw / SCALE_SET_POWER_KW);
    return inverterWrite(REG_SET_POWER, raw);
}

bool inverterPowerOn() {
    return inverterWrite(REG_POWER_ON, 1);
}

bool inverterShutdown() {
    return inverterWrite(REG_POWER_ON, 0);
}

// ---------------------------------------------------------------------------
// Poll
// ---------------------------------------------------------------------------

void pollModbus(InvData& inver) {
    int16_t raw[1];
    if (inverterRead(REG_STATUS, REG_STATUS_COUNT, raw)) {
        inverter_parse_status(raw, inver.status); // <--- Pasamos directo la subestructura interna
    } else Serial.println("[Inverter] Error: reg 32 (status)");

    int16_t ac_raw[REG_AC_COUNT];
    if (inverterRead(REG_AC_START, REG_AC_COUNT, ac_raw)) {
        inverter_parse_ac(ac_raw, inver.ac);
    } else Serial.println("[Inverter] Error: reg 100-125 (AC)");

    int16_t dc_raw[REG_DC_COUNT];
    if (inverterRead(REG_DC_START, REG_DC_COUNT, dc_raw)) {
        inverter_parse_dc(dc_raw, inver.dc);
    } else Serial.println("[Inverter] Error: reg 141-143 (DC)");

    int16_t grid_raw[REG_GRID_COUNT];
    int16_t grid_p_raw = 0;
    if (inverterRead(REG_GRID_START, REG_GRID_COUNT, grid_raw) &&
        inverterRead(REG_GRID_POWER, 1, &grid_p_raw)) {
        inverter_parse_grid(grid_raw, grid_p_raw, inver.grid);
    } else Serial.println("[Inverter] Error: reg 170-179 / 192 (grid)");

#ifdef INVERTER_PROTOCOL_V3
    int16_t load_raw[REG_LOAD_COUNT];
    if (inverterRead(REG_LOAD_START, REG_LOAD_COUNT, load_raw)) {
        inverter_parse_load(load_raw, inver.load);
    } else Serial.println("[Inverter] Error: reg 200-213 (load V3.0)");
#endif
}