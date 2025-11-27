#ifndef MEDIR_CORRENTE_H
#define MEDIR_CORRENTE_H

#include <Arduino.h>

// Estrutura para facilitar o transporte dos dados
struct DadosCorrente {
    float correnteAC1; // Valor RMS em Amperes
    float correnteAC2; // Valor RMS em Amperes
    float correnteAC3; // Valor RMS em Amperes
    float correnteDC;  // Valor médio em Amperes
};

// Protótipos das funções
void configurarSensoresCorrente();
void calibrarZeroCorrente(); // <--- Função crítica para corrigir o "offset"
DadosCorrente lerSensoresCorrente();

#endif