#ifndef MEDIR_TENSAO_H
#define MEDIR_TENSAO_H

#include <Arduino.h>

struct DadosTensao {
    float tensaoV;      // Tensão medida nos terminais do INA219 (0-26V)
};

void configurarSensorTensao();
DadosTensao lerSensorTensao();

#endif