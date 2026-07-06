// src\bms_can.cpp
#include "bms_can.h"

// Instanciamos la librería pasándole el pin CS desde el config.h
MCP2515 mcp2515(CAN_CS);

volatile bool nuevoMensajeCAN = false;

// ---------------------------------------------------------------------------
// LA ISR: Se ejecuta por hardware cuando el MCP2515 recibe su propio Eco
// ---------------------------------------------------------------------------
void IRAM_ATTR atenderInterrupcionCAN() {
    // Dentro de la ISR solo se levanta la bandera (flag).
    nuevoMensajeCAN = true;
}

// ---------------------------------------------------------------------------
// INICIALIZACIÓN: Configura el Eco por Hardware (Loopback)
// ---------------------------------------------------------------------------
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
    Serial.println("[CAN] MCP2515 configurado en MODO LOOPBACK (500 Kbps). Listo para escuchar el BMS real.");
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

// ---------------------------------------------------------------------------
// ENVIAR DATOS: El ESP32 le escribe al MCP2515
// ---------------------------------------------------------------------------
byte bmsSend(can_frame* ptr_msg) {
    // La idea es que se invoque esta funcion en el loop principal con el mensaje que se genera en 'mkMsg'
    return mcp2515.sendMessage(ptr_msg);
}

// ---------------------------------------------------------------------------
// LEER: Procesa el paquete cuando la ISR avisa que el pin INT bajó
// ---------------------------------------------------------------------------
bool bmsReceive(can_frame* ptr_msg, bool listening) {
    // Si la ISR levantó la bandera de interrupción
    if (nuevoMensajeCAN){
        nuevoMensajeCAN = false;
        if (listening){
            // Leemos el buffer de recepción por SPI de forma segura en el flujo principal
            mcp2515.readMessage(ptr_msg); // Se ve que readMessage recibe un puntero.
            // Segun entiendo cuando el MCP2515 baja la bandera de interrupcion y el esp32 le pide para leerlo
            // con 'readMessage' y esta funcion ya te lo deja en la estructura 'canMsgRx'
            return true;
        } 
    }
    return false;
}

void imprimirRx(can_frame* ptr_msg){
    //__u8 CAN_DLC = canMsgRx.can_dlc;
    Serial.print("[CAN] la id del frame es: ");
    Serial.println((*ptr_msg).can_id, HEX);

    Serial.print("[CAN] la cantidad de bytes recibidos fueron: ");
    Serial.println((*ptr_msg).can_dlc);

    __u8 length = (*ptr_msg).can_dlc;

    for(int i=0; i<length; i++){
        Serial.print("[CAN] el contenido del byte ");
        Serial.print(i+1);
        Serial.print(" es: ");
        Serial.println((*ptr_msg).data[i], HEX);
    }
}

// Esta funcion la hizo full el gemini, es para leer lo que se escribe en el monitor serie
bool procesarEntradaTeclado(String entrada, canid_t &idOut, __u8 *datosOut, __u8 &largoOut) {
    entrada.trim(); // Limpiamos espacios o caracteres raros
    if (entrada.length() == 0) return false;

    // Buscamos los dos puntos que separan el ID de los datos
    int indiceDosPuntos = entrada.indexOf(':');
    if (indiceDosPuntos == -1) {
        Serial.println("[ERROR] Formato inválido. Usar -> ID_HEX:D1_HEX,D2_HEX...");
        return false;
    }

    // 1. Extraemos y parseamos el ID (asumimos formato Hexadecimal)
    String strID = entrada.substring(0, indiceDosPuntos);
    idOut = strtoul(strID.c_str(), NULL, 16);

    // 2. Extraemos y parseamos los bytes separados por comas
    String strDatos = entrada.substring(indiceDosPuntos + 1);
    largoOut = 0;

    int posComa = 0;
    while ((posComa = strDatos.indexOf(',')) != -1 && largoOut < 8) {
        String byteStr = strDatos.substring(0, posComa);
        datosOut[largoOut++] = (__u8)strtol(byteStr.c_str(), NULL, 16);
        strDatos = strDatos.substring(posComa + 1);
    }
    // Parseamos el último byte (el que no tiene coma al final)
    if (strDatos.length() > 0 && largoOut < 8) {
        datosOut[largoOut++] = (__u8)strtol(strDatos.c_str(), NULL, 16);
    }

    return true;
}


void limpiarBuffers_Rx() {
    nuevoMensajeCAN = false;
    if (mcp2515.checkError()) {
        Serial.println("ALERT: [MCP2515] Se tiene overflow");
        
        //Aca se escriben los bit del registro de error.
        uint8_t flags = mcp2515.getErrorFlags();
        Serial.print("Registro EFLG (Código de error): 0x");
        Serial.println(flags, HEX);
        
        // Limpiamos el overflow para desbloquear la recepción
        mcp2515.clearRXnOVR();
        
        // Purgamos los buffers viejos borrando las banderas de interrupción
        mcp2515.clearInterrupts();
        Serial.println("-> Overflow y buffers reiniciados con éxito.");
    }    
}