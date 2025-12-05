#include "MedirCorrente.h"

// =================================================================================
// --- ÁREA DE CONFIGURAÇÃO DO USUÁRIO (SELECIONADO: ACS758LCB-100B) ---
// Atenção: O sensor é alimentado com 5V, mas o STM32 lê com referência de 3.3V.
// Isso cria um offset de ~2.5V (leitura ~3100 no ADC) no zero.

// --- MODELO ACS758LCB-100B (+- 100A) ---
// #define MODELO_100B_5V    // 20 mV/A (Padrão para este sensor a 5V)
//#define MODELO_100B_3V3   // ~13.2 mV/A (Apenas se VCC do sensor for 3.3V)

// --- CASO NENHUM SEJA SELECIONADO (SEGURANÇA) ---
#if !defined(MODELO_100B_5V)
    #define MODELO_100B_5V 
#endif
// =================================================================================

// --- DEFINIÇÃO DOS PINOS (ADC - STM32F407) ---
const int PIN_AC1 = PA1; 
const int PIN_AC2 = PA2;
const int PIN_AC3 = PA3;
const int PIN_DC  = PC4;

// --- CONSTANTES DE FÍSICA E AMOSTRAGEM ---
// Hardware STM32
const float ADC_VOLTAGE_REF = 3.3;  
const float ADC_SCALE = 4095.0;     

// Parâmetros da Lógica (Baseado no seu main.cpp)
const unsigned long SAMPLING_WINDOW_MS = 200; // Janela de 200ms conforme sua lógica
const float NOISE_DEADZONE_AMPS = 0.30;       // Zona morta de 0.3A

// --- SELEÇÃO AUTOMÁTICA DA SENSIBILIDADE ---
#if defined(MODELO_100B_5V)
  const float SENSIBILIDADE_V_A = 0.0132; 
#elif defined(MODELO_100B_3V3)
  const float SENSIBILIDADE_V_A = 0.0132;
#else
  const float SENSIBILIDADE_V_A = 0.020; 
#endif

// --- VARIÁVEIS DE CALIBRAÇÃO (OFFSET) ---
float offset_AC1 = 0;
float offset_AC2 = 0;
float offset_AC3 = 0;
float offset_DC  = 0;

void configurarSensoresCorrente() {
    analogReadResolution(12); // STM32 usa 12 bits (0-4095)
    pinMode(PIN_AC1, INPUT_ANALOG);
    pinMode(PIN_AC2, INPUT_ANALOG);
    pinMode(PIN_AC3, INPUT_ANALOG);
    pinMode(PIN_DC, INPUT_ANALOG);
}

// --- FUNÇÃO DE CALIBRAÇÃO (Média de 1000 amostras - Igual ao setup do main.cpp) ---
void calibrarZeroCorrente() {
    long somaAC1 = 0;
    long somaAC2 = 0;
    long somaAC3 = 0;
    long somaDC  = 0;
    int n = 1000;

    delay(1000); // Aguarda estabilização inicial

    for(int i=0; i<n; i++) {
        somaAC1 += analogRead(PIN_AC1);
        somaAC2 += analogRead(PIN_AC2);
        somaAC3 += analogRead(PIN_AC3);
        somaDC  += analogRead(PIN_DC);
        delay(1);
    }

    offset_AC1 = (float)somaAC1 / n;
    offset_AC2 = (float)somaAC2 / n;
    offset_AC3 = (float)somaAC3 / n;
    offset_DC  = (float)somaDC  / n;

    #if defined(USBCON)
      SerialUSB.println("--- CALIBRACAO CONCLUIDA (Logica Media 1000x) ---");
      SerialUSB.print("Offset AC1: "); SerialUSB.println(offset_AC1);
      SerialUSB.print("Offset AC2: "); SerialUSB.println(offset_AC2);
      SerialUSB.print("Offset AC3: "); SerialUSB.println(offset_AC3);
      SerialUSB.print("Offset DC:  "); SerialUSB.println(offset_DC);
    #endif
}

DadosCorrente lerSensoresCorrente() {
    DadosCorrente dados;
    
    unsigned long startTime = millis();
    long amostrasCount = 0;

    // Acumuladores para AC (Soma dos Quadrados da Corrente)
    // Baseado na lógica do main.cpp: sumOfSquares += current * current
    double somaQuadradaCurrent_AC1 = 0.0;
    double somaQuadradaCurrent_AC2 = 0.0;
    double somaQuadradaCurrent_AC3 = 0.0;

    // Acumulador para DC (Soma Bruta para Média)
    // Baseado na lógica do main.cpp: totalReadings += analogRead
    long somaLeituras_DC = 0;

    // --- LOOP DE AMOSTRAGEM (Janela de Tempo) ---
    while (millis() - startTime < SAMPLING_WINDOW_MS) {
        
        // 1. Leituras RAW
        int raw1 = analogRead(PIN_AC1);
        int raw2 = analogRead(PIN_AC2);
        int raw3 = analogRead(PIN_AC3);
        
        // 2. Cálculo da Corrente Instantânea (AC)
        // Lógica: (Raw - Offset) * (Vref / Scale) / Sensibilidade
        float i1 = ((float)raw1 - offset_AC1) * (ADC_VOLTAGE_REF / ADC_SCALE) / SENSIBILIDADE_V_A;
        float i2 = ((float)raw2 - offset_AC2) * (ADC_VOLTAGE_REF / ADC_SCALE) / SENSIBILIDADE_V_A;
        float i3 = ((float)raw3 - offset_AC3) * (3.3 / ADC_SCALE) / SENSIBILIDADE_V_A;

        // 3. Acumula o quadrado da corrente
        somaQuadradaCurrent_AC1 += (double)i1 * (double)i1;
        somaQuadradaCurrent_AC2 += (double)i2 * (double)i2;
        somaQuadradaCurrent_AC3 += (double)i3 * (double)i3;

        // 4. Leitura DC (Apenas acumula o RAW, conforme sua lógica)
        somaLeituras_DC += analogRead(PIN_DC);
        
        amostrasCount++;
    }

    if (amostrasCount == 0) amostrasCount = 1; // Proteção div por zero

    // --- CÁLCULOS FINAIS ---

    // 1. AC: Cálculo do RMS = Raiz(Media(Quadrados))
    dados.correnteAC1 = sqrt(somaQuadradaCurrent_AC1 / (double)amostrasCount);
    dados.correnteAC2 = sqrt(somaQuadradaCurrent_AC2 / (double)amostrasCount);
    dados.correnteAC3 = sqrt(somaQuadradaCurrent_AC3 / (double)amostrasCount);

    // 2. DC: Cálculo da Média e Conversão Final
    float mediaRawDC = (float)somaLeituras_DC / (float)amostrasCount;
    // (MediaRaw - Offset) * FatorVolts / Sensibilidade
    float tensaoDiferencaDC = (mediaRawDC - offset_DC) * (ADC_VOLTAGE_REF / ADC_SCALE);
    dados.correnteDC = tensaoDiferencaDC / SENSIBILIDADE_V_A;

    // --- FILTRO DE ZONA MORTA (Noise Gate) ---
    // Conforme definido no seu main.cpp (#define NOISE_DEADZONE_AMPS 0.3)
    if (dados.correnteAC1 < NOISE_DEADZONE_AMPS) dados.correnteAC1 = 0.0;
    if (dados.correnteAC2 < NOISE_DEADZONE_AMPS) dados.correnteAC2 = 0.0;
    if (dados.correnteAC3 < NOISE_DEADZONE_AMPS) dados.correnteAC3 = 0.0;
    if (abs(dados.correnteDC) < NOISE_DEADZONE_AMPS) dados.correnteDC = 0.0;

    return dados;
}