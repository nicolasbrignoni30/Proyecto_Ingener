// include\bms_can.h
#pragma once
#include <Arduino.h>
#include "config.h"
#include <SPI.h>
#include <mcp2515.h> // Esta en el platformio.ini


#define LISTEN_INTERVAL_BMS  5000
// ---------------------------------------------------------------------------
// Variables globales compartidas (Variables externas)
// El main.cpp y el display.cpp van a poder leer estos valores frescos del CAN
// ---------------------------------------------------------------------------
extern volatile float bms_v;
extern volatile float bms_i;
extern volatile float bms_soc;
extern volatile bool nuevoMensajeCAN;

// Se definen los posibles modos de funcionamiento
enum ModoFuncionamiento {
    MODO_NORMAL,
    MODO_LOOPBACK
};

// ---------------------------------------------------------------------------
// Interfaz del Módulo CAN (Funciones públicas)
// ---------------------------------------------------------------------------
void bmsCanInit(ModoFuncionamiento modo);                     // Inicializa el MCP2515 y la interrupción
void mkMsg(canid_t ID, __u8* bytes, __u8 length, can_frame* ptr_msg);
byte bmsSend(can_frame* ptr_msg);
bool bmsReceive(can_frame* ptr_msg, bool listening);
void imprimir_0x421();
void imprimirRx(can_frame* ptr_msg);
bool procesarEntradaTeclado(String entrada, canid_t &idOut, __u8 *datosOut, __u8 &largoOut);
void limpiarBuffers_Rx();
