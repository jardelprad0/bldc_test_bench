#include "MedirRPM.h"

// Variáveis voláteis são necessárias pois são alteradas dentro da interrupção
volatile unsigned long contadorPulsos = 0;
unsigned long ultimoTempoCalculo = 0;
float velocidadeRPM = 0.0f;

// --- FUNÇÃO DE INTERRUPÇÃO (ISR) ---
// Executada automaticamente toda vez que o sinal muda no pino
void contarPulsosISR() {
    contadorPulsos++;
}

// --- CONFIGURAÇÃO ---
void configurarRPM() {
    pinMode(PINO_RPM, INPUT_PULLUP); // Usa resistor interno se for coletor aberto
    // CHANGE conta subida e descida (2x pulsos). 
    // RISING conta apenas subida (1x pulso). 
    // O seu exemplo original usava CHANGE.
    attachInterrupt(digitalPinToInterrupt(PINO_RPM), contarPulsosISR, CHANGE);
    
    ultimoTempoCalculo = millis();
}

// --- LEITURA DO RPM ---
DadosRPM lerRPM() {
    DadosRPM dados;
    unsigned long tempoAtual = millis();
    
    // Verifica se já passou o tempo definido (ex: 500ms)
    if (tempoAtual - ultimoTempoCalculo >= INTERVALO_LEITURA_RPM) {
        
        // Desabilita interrupções momentaneamente para ler e zerar a variável com segurança
        noInterrupts();
        unsigned long pulsosCapturados = contadorPulsos;
        contadorPulsos = 0;
        interrupts();
        
        unsigned long deltaTempo = tempoAtual - ultimoTempoCalculo;
        ultimoTempoCalculo = tempoAtual;

        // CÁLCULO FÍSICO (Baseado no seu exemplo)
        // RPM = (Pulsos / PulsosPorVolta) * (60000ms / deltaTempo)
        // O Fator 1000.0f / deltaTempo converte para segundos
        // O Fator 60.0f converte segundos para minutos
        
        // Se usar CHANGE no interrupt, lembre que ele conta 2x por pulso elétrico,
        // então talvez precise ajustar PULSOS_POR_VOLTA.
        
        velocidadeRPM = ((float)pulsosCapturados / PULSOS_POR_VOLTA) * (60000.0f / deltaTempo);
        
        // Preenche retorno
        dados.pulsos = pulsosCapturados;
    }
    
    // Retorna o último valor calculado (ou o novo se entrou no if)
    dados.rpm = velocidadeRPM;
    
    // Se não entrou no IF, retorna pulsos como 0 apenas para visualização, 
    // mas mantém o valor de RPM estável até a próxima janela de tempo.
    if (tempoAtual - ultimoTempoCalculo < INTERVALO_LEITURA_RPM) {
         dados.pulsos = contadorPulsos; // Mostra contagem em tempo real para debug
    }

    return dados;
}