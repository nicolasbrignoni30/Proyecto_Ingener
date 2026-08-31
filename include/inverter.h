#pragma once
#include <Arduino.h>
#include <config.h>
#include <ModbusMaster.h>
#include "inverter_parser.h"
#include "inverter_scales.h"


//Registros 
#define REG_DC_MAX_DISCHG_CURRENT  763
#define REG_DC_MAX_CHG_CURRENT     764
#define REG_3PHASE_CTRL_MODE       341
#define REG_PV_SWITCH              652
#define REG_LEAKAGE_DETECT         795
#define REG_DCDC_SWITCH            656
#define REG_POWER_ON               650
#define REG_SHUTDOWN               651
//#define REG_FUNCTION_MGMT          873   
#define REG_ANTI_BACKFLOW          873   
#define REG_GRID_SCHED_MODE        758   
#define REG_SET_POWER              353 

// Estos registros estaban en el config.h y me los traje para aca para que quede mas prolijo
// No se si todos se usan y algunos son de la version V3.0.
// Lectura
#define REG_STATUS                  32
#define REG_STATUS_COUNT             1
#define REG_AC_START               100
#define REG_AC_COUNT                26
#define REG_DC_START               141
#define REG_DC_COUNT                 3
#define REG_GRID_START             170
#define REG_GRID_COUNT              10
#define REG_GRID_POWER             192
#define REG_LOAD_START             200
#define REG_LOAD_COUNT              14
#define REG_VERSION_START            0
#define REG_VERSION_COUNT           22


struct InitCmd {
    uint16_t    reg;
    int16_t     val;
    const char* name;
};

struct inverterValues{
    float   dc_max_dischg_current;
    float   dc_max_chg_current;
    int16_t anti_backflow_value;
    int16_t grid_sched_mode_value;
    int16_t three_phase_ctrl_mode_value;
    int16_t pv_switch_value;
    int16_t leakage_detect_value;
    int16_t dcdc_switch_value;
    float   set_power;
    int16_t power_on_value;
};

extern inverterValues inverter_values;

bool inverterWrite(uint16_t reg, int16_t value);
bool inverterRead(uint16_t reg, uint16_t count, int16_t* out);
void inverterInit(HardwareSerial& serial, uint8_t deRePin);
void inverter_init_defaults();
void inverter_reinit_from_cloud();
void inverter_update_reg_values(const String& key, float value);
void verifyAndReinit();
void readFirmwareVersion(FirmData& firm);
bool inverterSetPower(float kw);
bool inverterPowerOn();
bool inverterShutdown();
void pollModbus(InvData& inv);
