#include "gas_alarm.h"
#include "config.h"
#include <ModbusMaster.h>

static ModbusMaster gas;
static uint8_t _gasDeRePin;

static void gasPreTransmission()  { digitalWrite(_gasDeRePin, HIGH); }
static void gasPostTransmission() { digitalWrite(_gasDeRePin, LOW);  }

static const char* gasErrorStr(uint8_t code) {
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

void gasAlarmInit(HardwareSerial& serial, uint8_t deRePin) {
    _gasDeRePin = deRePin;
    pinMode(deRePin, OUTPUT);
    digitalWrite(deRePin, LOW);

    gas.begin(GAS_DEVICE_ID, serial);
    gas.preTransmission(gasPreTransmission);
    gas.postTransmission(gasPostTransmission);
}

bool gasAlarmRead(uint16_t reg, uint16_t count, uint16_t* out) {
    uint8_t result = gas.readHoldingRegisters(reg, count);
    if (result != ModbusMaster::ku8MBSuccess) {
        Serial.printf("[Gas] Read reg %d count %d failed: 0x%02X — %s\n",
                      reg, count, result, gasErrorStr(result));
        return false;
    }
    for (uint16_t i = 0; i < count; i++)
        out[i] = gas.getResponseBuffer(i);
    return true;
}

bool gasAlarmReadAll(GasData& out) {
    uint16_t raw[GAS_REG_ALL_COUNT];
    if (!gasAlarmRead(GAS_REG_ALARM1, GAS_REG_ALL_COUNT, raw)) {
        return false;
    }
    gas_parse_all(raw, out);
    return true;
}