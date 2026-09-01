// include\bms_can.h
#pragma once
#include <Arduino.h>
#include "config.h"
#include <SPI.h>
#include <mcp2515.h> 

extern volatile bool nuevoMensajeCAN;

// Se definen los posibles modos de funcionamiento
enum ModoFuncionamiento {
    MODO_NORMAL,
    MODO_LOOPBACK
};

// ---------------------------------------------------------------------------
// Interfaz del Módulo CAN (Funciones públicas)
// ---------------------------------------------------------------------------
void bmsCanInit(ModoFuncionamiento modo);                    
byte bmsSend(can_frame* ptr_msg);
bool bmsReceive(can_frame* ptr_msg);
bool bmsReceiveBatch(can_frame* ptr_msg, uint8_t num_frames, unsigned long timeout_ms, void (*onFrame)(can_frame*));
