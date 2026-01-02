#include "ControlarMotor.h"

Servo pwmEsc;

// Variáveis de Controle da Máquina de Estados
EstadoMotor estado = EstadoMotor::STOPBIT;
bool motorLigado = false;
bool sentidoSubida = true; // true = subindo (0->100), false = descendo (100->0)

// Variáveis de Valor
int percentualAlvoAtual = 0; // O "degrau" atual (0, 5, 10...)
float pwmAtual = 1000;       // PWM sendo enviado agora (pode estar no meio da rampa)
float pwmDesejado = 1000;    // PWM alvo do degrau atual

// Temporizadores
unsigned long timeRamp = 0;
unsigned long timeWait = 0;
const int delayRamp = 1; // Suavidade da transição entre degraus

// --- FUNÇÕES AUXILIARES ---
int converterPercentParaPwm(int percent) {
    // Garante limites 0-100
    if (percent < 0) percent = 0;
    if (percent > 80) percent = 80;
    return map(percent, 0, 80, PWM_MIN, PWM_MAX);
}

// --- FUNÇÕES PRINCIPAIS ---

void configurarMotor() {
    pinMode(PINO_BOTAO_START, INPUT_PULLUP);

    pwmEsc.attach(PINO_MOTOR, PWM_MIN, PWM_MAX);
    
    // Armamento do ESC (Sinal baixo por 3s)
    pwmEsc.writeMicroseconds(PWM_MIN);
    pwmAtual = PWM_MIN;
    delay(3000); 
    
    estado = EstadoMotor::STOPBIT;
    motorLigado = false;
}

void alternarEstadoSequencia() {
    motorLigado = !motorLigado;
    
    if (motorLigado) {
        // Reinicia a sequência
        estado = EstadoMotor::STARTBIT;
        sentidoSubida = true; // Começa subindo
        percentualAlvoAtual = 0; // Começa do 0
    } else {
        // Força parada
        estado = EstadoMotor::STOPBIT;
    }
}

DadosMotor processarMotor() {
    DadosMotor dados;
    unsigned long agora = millis();

    // Segurança: Se desligaram a flag, vai para STOP
    if (!motorLigado) {
        estado = EstadoMotor::STOPBIT;
    }

    switch (estado) {
        case EstadoMotor::STARTBIT:
            percentualAlvoAtual = 0;
            sentidoSubida = true;
            estado = EstadoMotor::SETPOINT;
            break;

        case EstadoMotor::SETPOINT:
            // CÁLCULO DO PRÓXIMO DEGRAU
            if (sentidoSubida) {
                percentualAlvoAtual += TAMANHO_DEGRAU_PERCENTUAL; // Sobe 5%
                if (percentualAlvoAtual >= 80) {
                    percentualAlvoAtual = 80;
                    sentidoSubida = false; // Atingiu o topo, próxima vez começa a descer
                }
            } else {
                percentualAlvoAtual -= TAMANHO_DEGRAU_PERCENTUAL; // Desce 5%
                if (percentualAlvoAtual <= 0) {
                    percentualAlvoAtual = 0;
                    // Se chegou a 0 descendo, o próximo passo será encerrar
                }
            }

            // Converte % para microssegundos
            pwmDesejado = converterPercentParaPwm(percentualAlvoAtual);
            
            // Prepara timers
            timeRamp = agora;
            estado = EstadoMotor::RAMPA;
            break;

        case EstadoMotor::RAMPA:
            // Executa a transição suave para o novo degrau
            if (agora - timeRamp >= delayRamp) {
                timeRamp = agora;

                // Passo de incremento do PWM (5us)
                int step = 0;
                if (pwmDesejado > pwmAtual) step = 5;
                else if (pwmDesejado < pwmAtual) step = -5;

                // Se ainda não chegou no alvo
                if (abs(pwmAtual - pwmDesejado) > 4) {
                    pwmAtual += step;
                    
                    // Limites de segurança
                    if (pwmAtual > PWM_MAX) pwmAtual = PWM_MAX;
                    if (pwmAtual < PWM_MIN) pwmAtual = PWM_MIN;

                    pwmEsc.writeMicroseconds((int)pwmAtual);
                } else {
                    // Chegou no degrau!
                    pwmAtual = pwmDesejado;
                    pwmEsc.writeMicroseconds((int)pwmAtual);
                    
                    timeWait = agora; // Inicia contagem do tempo de espera
                    estado = EstadoMotor::ESPERA;
                }
            }
            break;

        case EstadoMotor::ESPERA:
            // Fica parado no degrau pelo tempo configurado (5s)
            if (agora - timeWait >= TEMPO_EM_CADA_DEGRAU_MS) {
                
                // O tempo acabou. O que fazer agora?
                
                // Se chegamos a 0% e estávamos descendo, acabou a sequência.
                if (percentualAlvoAtual == 0 && !sentidoSubida) {
                    motorLigado = false;
                    estado = EstadoMotor::STOPBIT;
                } else {
                    // Senão, calcula o próximo degrau
                    estado = EstadoMotor::SETPOINT;
                }
            }
            break;

        case EstadoMotor::STOPBIT:
            motorLigado = false;
            percentualAlvoAtual = 0;
            sentidoSubida = true;
            
            // Garante motor desligado
            if (pwmAtual != PWM_MIN) {
                pwmAtual = PWM_MIN;
                pwmEsc.writeMicroseconds(PWM_MIN);
            }
            break;
    }

    // Retorno para Monitoramento
    dados.pwmMicroseconds = (int)pwmAtual;
    dados.aceleracaoPercentual = map((long)pwmAtual, PWM_MIN, PWM_MAX, 0, 80);
    dados.setpointAtual = percentualAlvoAtual; // Mostra qual degrau estamos buscando/mantendo
    dados.ativo = motorLigado;
    
    switch (estado) {
        case EstadoMotor::STARTBIT: dados.estadoAtual = "START"; break;
        case EstadoMotor::SETPOINT: dados.estadoAtual = "CALC_DEG"; break;
        case EstadoMotor::RAMPA:    dados.estadoAtual = "RAMPA"; break;
        case EstadoMotor::ESPERA:   dados.estadoAtual = "PATAMAR_5S"; break;
        case EstadoMotor::STOPBIT:  dados.estadoAtual = "STOP"; break;
    }

    return dados;
}