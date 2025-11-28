#include "MedirTensao.h"
#include <Wire.h>
#include <Adafruit_INA219.h>

// Instância do sensor
Adafruit_INA219 ina219;
bool sensorTensaoOK = false;

// Definição dos pinos I2C para STM32F407VE (Black Board)
// Isso garante que estamos usando o I2C1 nos pinos corretos.
#define I2C_SDA PB7
#define I2C_SCL PB6

void configurarSensorTensao() {
    // Inicializa o barramento I2C nos pinos específicos antes de iniciar o sensor
    Wire.setSDA(I2C_SDA);
    Wire.setSCL(I2C_SCL);
    Wire.begin();

    // Inicializa o sensor I2C. 
    // O INA219 mede tensão no pino Vin- (em relação ao GND)
    if (!ina219.begin()) {
        #if defined(USBCON)
            SerialUSB.println("ERRO: Sensor de Tensao (INA219) nao encontrado! Verifique PB6(SCL) e PB7(SDA).");
        #else
            Serial.println("ERRO: Sensor de Tensao (INA219) nao encontrado! Verifique PB6(SCL) e PB7(SDA).");
        #endif
        sensorTensaoOK = false;
    } else {
        // Calibração padrão (32V, 2A) é suficiente para ler apenas tensão bus corretamente
        // mesmo que não usemos a leitura de corrente interna dele.
        sensorTensaoOK = true;
    }
}

DadosTensao lerSensorTensao() {
    DadosTensao dados;
    dados.tensaoV = 0.0;

    if (sensorTensaoOK) {
        // getBusVoltage_V() retorna a tensão no pino Vin- (carga)
        dados.tensaoV = ina219.getBusVoltage_V();
    }

    return dados;
}