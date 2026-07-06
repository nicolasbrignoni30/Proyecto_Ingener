#pragma once
#include <Arduino.h>
#include <config.h>
#include <ModbusMaster.h>
#include "inverter_parser.h"

void inverterInit(HardwareSerial& serial, uint8_t deRePin);
void readFirmwareVersion(FirmData& firm);
void verifyAndReinit();
void pollModbus(InvData& inv);
bool inverterSetPower(float kw);
bool inverterPowerOn();
bool inverterShutdown();
bool inverterReadRaw(uint16_t reg, int16_t* out);  // diagnostic — read single register
