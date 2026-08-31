#include "bms_can.h"

// Instanciamos la librería pasándole el pin CS desde el config.h
MCP2515 mcp2515(CAN_CS);

volatile bool nuevoMensajeCAN = false;

namespace{
    // ISR
    void IRAM_ATTR atenderInterrupcionCAN() {
        // Dentro de la ISR solo se levanta la bandera (flag).
        nuevoMensajeCAN = true;
    }

    void mkMsg(canid_t ID, __u8* bytes, __u8 length, can_frame* ptr_msg){
        if (ptr_msg != NULL) {
            (*ptr_msg).can_id = ID;
            (*ptr_msg).can_dlc = length;

            for (int i=0; i<length; i++){
                (*ptr_msg).data[i] = bytes[i];
            }
        }
    }

    void limpiarBuffers_Rx() {
        nuevoMensajeCAN = false;
        if (mcp2515.checkError()) {
            Serial.println("ALERT: [MCP2515] Se tiene overflow");
            
            //Aca se escriben los bit del registro de error.
            uint8_t flags = mcp2515.getErrorFlags();
            Serial.print("Registro EFLG (Código de error): 0x");
            Serial.println(flags, HEX);
        }
        // Limpiamos el overflow para desbloquear la recepción
        mcp2515.clearRXnOVR();
        
        // Purgamos los buffers viejos borrando las banderas de interrupción
        mcp2515.clearInterrupts();
    }
}


void bmsCanInit(ModoFuncionamiento modo) {
    pinMode(CAN_INT, INPUT_PULLUP); // CAN_INT (ver config.h) se pone como entrada con resistencia de pullup ==> normalemente en 3.3V
    attachInterrupt(digitalPinToInterrupt(CAN_INT), atenderInterrupcionCAN, FALLING); // Aca le dice que para las interrupciones use el GPIO CAN_INT y que salta a la isr apenas baje (falling edge)

    //SPI.begin(); // Inicializa el bus SPI compartido. Se hace en main.cpp
    
    mcp2515.reset();
    // Configura baudrate 500Kbps y el cristal de tu placa (en nuestro caso es de 8MHz)
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ); 
    
    // modo de echo para simular que el BMS habla
    if (modo == MODO_LOOPBACK){
        mcp2515.setLoopbackMode(); 
    }
    if (modo == MODO_NORMAL){
        mcp2515.setNormalMode();
    }
    Serial.println("[CAN] MCP2515 configurado.");
}


byte bmsSend(canid_t ID, __u8* bytes, __u8 length, can_frame* ptr_msg) {
    mkMsg(ID, bytes, length, ptr_msg);
    return mcp2515.sendMessage(ptr_msg);
}

bool bmsReceive(can_frame* ptr_msg) {
    if (nuevoMensajeCAN){
        nuevoMensajeCAN = false;
        mcp2515.readMessage(ptr_msg);
        return true;
    }
    return false;
}


bool bmsReceiveBatchBlocking(can_frame* ptr_msg, uint8_t num_frames, unsigned long timeout_ms, void (*onFrame)(can_frame*)) {
    limpiarBuffers_Rx();
 
    unsigned long inicio = millis();
    uint8_t contador = 0;
 
    while (contador < num_frames) {
        if (bmsReceive(ptr_msg)) {
            onFrame(ptr_msg);
            contador++;
        }
 
        if (millis() - inicio >= timeout_ms) {
            return false; // timeout de seguridad, lote incompleto
            
        }
    }
 
    return true;
}