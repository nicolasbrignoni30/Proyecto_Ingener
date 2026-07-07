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

bool inverterReadRaw(uint16_t reg, int16_t* out) {
    return inverterRead(reg, 1, out);
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

// Estas dos funciones las traje del inverter_parser, no tenian porque estar ahi.

const InitCmd* inverter_init_sequence(uint8_t* count_out) {
    static const InitCmd seq[] = {
        // Hardware limits
        { REG_DC_MAX_DISCHG_CURRENT, 1500, "Max DC discharge = 150A"         },
        { REG_DC_MAX_CHG_CURRENT,    1500, "Max DC charge = 150A"             },
        // Operating mode — on-grid, no PV, no self-use
        { REG_ANTI_BACKFLOW,            0, "Self-use OFF (on-grid mode)"      },
        { REG_GRID_SCHED_MODE,          0, "AC side constant power (reg 758)" },
        { REG_3PHASE_CTRL_MODE,         1, "3-phase independent control"      },
        { REG_PV_SWITCH,                0, "PV OFF"                           },
        { REG_LEAKAGE_DETECT,           0, "Leakage detect OFF"               },
        { REG_DCDC_SWITCH,              0, "DCDC OFF"                         },
        // Setpoint to zero before power-on
        { REG_SET_POWER,                0, "Setpoint = 0 kW"                  },
        // Power on last
        { REG_POWER_ON,                 1, "Power ON"                         },
    };
    *count_out = sizeof(seq) / sizeof(seq[0]); //Manera rara de contar la cantidad de struct InitCmd en seq.
    return seq;
}

bool inverter_run_init() {
    uint8_t count;
    const InitCmd* seq = inverter_init_sequence(&count);
    bool ok = true;
    for (uint8_t i = 0; i < count; i++)
        ok &= inverterWrite(seq[i].reg, seq[i].val);
    return ok;
}

void inverterInit(HardwareSerial& serial, uint8_t deRePin) {
    _deRePin = deRePin;
    pinMode(deRePin, OUTPUT);
    digitalWrite(deRePin, LOW);
    inv.begin(MODBUS_DEVICE_ID, serial);
    inv.preTransmission(preTransmission);
    inv.postTransmission(postTransmission);

    Serial.println("[Inverter] Configurando...");
    bool ok = inverter_run_init();
    Serial.printf("[Inverter] Init %s\n", ok ? "OK" : "WARNING: algún registro falló");
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
// Verify and reinit
// ---------------------------------------------------------------------------
void verifyAndReinit() {
    static const InitCmd cfg[] = {
        { 763, 1500, "Max DC discharge"         },
        { 764, 1500, "Max DC charge"            },
        { 341,    1, "3-phase ctrl"             },
        { 652,    0, "PV switch"                },
        { 795,    0, "Leakage detect"           },
        { 656,    0, "DCDC switch"              },
        { 873,    0, "Function mgmt (on-grid)"  },
        { 758,    0, "Grid sched mode (AC pwr)" },
    };
    bool anyFixed = false;
    for (const InitCmd& c : cfg) {
        int16_t cur;
        if (!inverterRead(c.reg, 1, &cur)){
            Serial.printf("[Verify] Error reg %d\n", c.reg); continue; 
        }
        if (cur != c.val) {
            Serial.printf("[Verify] reg %d: %d → %d\n", c.reg, cur, c.val);
            inverterWrite(c.reg, c.val);
            anyFixed = true;
            delay(100);
        }
    }
    Serial.println(anyFixed ? "[Verify] Parámetros corregidos" : "[Verify] Config OK");
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

void pollModbus(InvData& inv) {
    int16_t raw[1];
    if (inverterRead(REG_STATUS, REG_STATUS_COUNT, raw)) {
        inverter_parse_status(raw, inv.status); // <--- Pasamos directo la subestructura interna
    } else Serial.println("[Inverter] Error: reg 32 (status)");

    int16_t ac_raw[REG_AC_COUNT];
    if (inverterRead(REG_AC_START, REG_AC_COUNT, ac_raw)) {
        inverter_parse_ac(ac_raw, inv.ac);
    } else Serial.println("[Inverter] Error: reg 100-125 (AC)");

    int16_t dc_raw[REG_DC_COUNT];
    if (inverterRead(REG_DC_START, REG_DC_COUNT, dc_raw)) {
        inverter_parse_dc(dc_raw, inv.dc);
    } else Serial.println("[Inverter] Error: reg 141-143 (DC)");

    int16_t grid_raw[REG_GRID_COUNT];
    int16_t grid_p_raw = 0;
    if (inverterRead(REG_GRID_START, REG_GRID_COUNT, grid_raw) &&
        inverterRead(REG_GRID_POWER, 1, &grid_p_raw)) {
        inverter_parse_grid(grid_raw, grid_p_raw, inv.grid);
    } else Serial.println("[Inverter] Error: reg 170-179 / 192 (grid)");

#ifdef INVERTER_PROTOCOL_V3
    int16_t load_raw[REG_LOAD_COUNT];
    if (inverterRead(REG_LOAD_START, REG_LOAD_COUNT, load_raw)) {
        inverter_parse_load(load_raw, inv.load);
    } else Serial.println("[Inverter] Error: reg 200-213 (load V3.0)");
#endif
}