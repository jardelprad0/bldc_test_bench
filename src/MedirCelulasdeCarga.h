#ifndef MEDIR_CELULAS_H
#define MEDIR_CELULAS_H

#include <Arduino.h>

// Estrutura para facilitar o transporte dos dados
struct DadosMedicao {
    float torqueNm;      // Torque calculado em N.m
    float rawTorqueG;    // Leitura bruta em gramas (Torque)
    float thrustKg;      // Força calculada em Kg
    float rawThrustG;    // Leitura bruta em gramas (Thrust)
};

// Protótipos das funções
void configurarCelulas();
DadosMedicao lerDadosSensores();

#endif