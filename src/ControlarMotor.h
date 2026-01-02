#ifndef CONTROLAR_MOTOR_H
#define CONTROLAR_MOTOR_H

#include <Arduino.h>
#include <Servo.h>

// --- CONFIGURAÇÃO FÍSICA ---
#define PINO_MOTOR PD13 
#define PINO_BOTAO_START PE4

// Faixa de PWM
#define PWM_MIN 1500 
#define PWM_MAX 1900 

// --- CONFIGURAÇÃO DA ESCADA AUTOMÁTICA ---
#define TAMANHO_DEGRAU_PERCENTUAL 10  // Incremento de 5%
#define TEMPO_EM_CADA_DEGRAU_MS 2000 // 5 segundos em cada patamar

// Estados da Máquina
enum class EstadoMotor {
    STARTBIT,
    SETPOINT,
    RAMPA,
    ESPERA,
    STOPBIT
};

// Estrutura de Retorno
struct DadosMotor {
    float aceleracaoPercentual; // Valor real atual (0-100%)
    int pwmMicroseconds;        // Valor PWM real enviado
    String estadoAtual;         // Texto para debug
    int setpointAtual;          // Qual é o alvo atual da escada (ex: 5, 10, 15...)
    bool ativo;                 // Se a sequência está rodando
};

// Protótipos
void configurarMotor();
void alternarEstadoSequencia(); 
DadosMotor processarMotor();

#endif