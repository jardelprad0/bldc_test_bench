#ifndef MEDIR_RPM_H
#define MEDIR_RPM_H

#include <Arduino.h>

// --- CONFIGURAÇÃO FÍSICA ---
// Escolha um pino que suporte interrupção. 
// No STM32F407, a maioria dos pinos digitais suporta.
// Verifique se PC6 está livre no seu hardware.
#define PINO_RPM PD12 

// Número de imãs ou pulsos por volta do motor
// O exemplo original usava 7.0f. Ajuste conforme seu motor.
#define PULSOS_POR_VOLTA 14.0f 

// Intervalo de tempo para calcular o RPM (em milissegundos)
// 1000ms = 1 segundo (atualização mais lenta, mais precisa)
// 250ms = 0.25 segundos (atualização mais rápida)
#define INTERVALO_LEITURA_RPM 500 

// Estrutura para transportar os dados
struct DadosRPM {
    float rpm;           // Rotações por minuto
    unsigned long pulsos; // Contagem bruta de pulsos (para debug)
};

// Protótipos
void configurarRPM();
void contarPulsosISR(); // Função de Interrupção
DadosRPM lerRPM();

#endif