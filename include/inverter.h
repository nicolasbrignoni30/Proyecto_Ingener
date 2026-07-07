#pragma once
#include <Arduino.h>
#include <config.h>
#include <ModbusMaster.h>
#include "inverter_parser.h"
#include "inverter_scales.h"

#define SET_POWER_KW      2.0f
#define SET_POWER_RAW     ((int16_t)(SET_POWER_KW / SCALE_SET_POWER_KW))

//Registros
#define REG_DC_MAX_DISCHG_CURRENT  763
#define REG_DC_MAX_CHG_CURRENT     764
#define REG_3PHASE_CTRL_MODE       341
#define REG_PV_SWITCH              652
#define REG_LEAKAGE_DETECT         795
#define REG_DCDC_SWITCH            656
#define REG_POWER_ON               650
#define REG_FUNCTION_MGMT          873   
#define REG_ANTI_BACKFLOW          873   
#define REG_GRID_SCHED_MODE        758   
#define REG_SET_POWER              353   

struct InitCmd {
    uint16_t    reg;
    int16_t     val;
    const char* name;
};

bool inverterWrite(uint16_t reg, int16_t value);
bool inverterRead(uint16_t reg, uint16_t count, int16_t* out);
void inverterInit(HardwareSerial& serial, uint8_t deRePin);
void readFirmwareVersion(FirmData& firm);
void verifyAndReinit();
void pollModbus(InvData& inv);
bool inverterSetPower(float kw);
bool inverterPowerOn();
bool inverterShutdown();
bool inverterReadRaw(uint16_t reg, int16_t* out);  // diagnostic — read single register
