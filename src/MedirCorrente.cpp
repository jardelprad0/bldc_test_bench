#include "MedirCorrente.h"

// =================================================================================
// --- ÁREA DE CONFIGURAÇÃO DO USUÁRIO (SELECIONADO: ACS758LCB-100B) ---
// Atenção: Certifique-se de que o sensor está alimentado com 5V.
// Se estiver alimentado com 3.3V (fora do spec), descomente a opção _3V3 e comente a _5V.

// --- MODELO ACS758LCB-050B (+- 50A) ---
// #define MODELO_050B_5V    // 40 mV/A
// #define MODELO_050B_3V3   // ~26.4 mV/A

// --- MODELO ACS758LCB-100B (+- 100A) ---
#define MODELO_100B_5V    // 20 mV/A (Padrão para este sensor a 5V)
// #define MODELO_100B_3V3   // ~13.2 mV/A (Apenas se VCC do sensor for 3.3V)

// --- MODELO ACS758KCB-150B (+- 150A) ---
// #define MODELO_150B_5V    // 13.3 mV/A
// #define MODELO_150B_3V3   // ~8.8 mV/A

// --- MODELO ACS758ECB-200B (+- 200A) ---
// #define MODELO_200B_5V    // 10 mV/A
// #define MODELO_200B_3V3   // ~6.6 mV/A

// --- CASO NENHUM SEJA SELECIONADO (SEGURANÇA) ---
#if !defined(MODELO_050B_5V) && !defined(MODELO_050B_3V3) && \
    !defined(MODELO_100B_5V) && !defined(MODELO_100B_3V3) && \
    !defined(MODELO_150B_5V) && !defined(MODELO_150B_3V3) && \
    !defined(MODELO_200B_5V) && !defined(MODELO_200B_3V3)
    #define MODELO_100B_5V 
#endif
// =================================================================================

// --- DEFINIÇÃO DOS PINOS (ADC - STM32F407) ---
const int PIN_AC1 = PA1; 
const int PIN_AC2 = PA2;
const int PIN_AC3 = PA3;
const int PIN_DC  = PC4;

// --- CONSTANTES DE FÍSICA ---
const float ADC_VOLTAGE_REF = 3.3; 
const float ADC_SCALE = 4095.0;     

// --- SELEÇÃO AUTOMÁTICA DA SENSIBILIDADE ---
#if defined(MODELO_050B_5V)
  const float SENSIBILIDADE_V_A = 0.040;
#elif defined(MODELO_050B_3V3)
  const float SENSIBILIDADE_V_A = 0.0264;
#elif defined(MODELO_100B_5V)
  const float SENSIBILIDADE_V_A = 0.020; // <--- SELECIONADO: 0.020 V/A
#elif defined(MODELO_100B_3V3)
  const float SENSIBILIDADE_V_A = 0.0132;
#elif defined(MODELO_150B_5V)
  const float SENSIBILIDADE_V_A = 0.0133;
#elif defined(MODELO_150B_3V3)
  const float SENSIBILIDADE_V_A = 0.0088;
#elif defined(MODELO_200B_5V)
  const float SENSIBILIDADE_V_A = 0.010;
#elif defined(MODELO_200B_3V3)
  const float SENSIBILIDADE_V_A = 0.0066;
#endif

// Zona morta: Correntes abaixo disto serão zeradas para evitar ruído
const float DEADZONE_A = 0.30; 

// --- VARIÁVEIS DE CALIBRAÇÃO (OFFSET) ---
// O valor RAW de zero amperes será determinado na inicialização
int offset_AC1 = 0;
int offset_AC2 = 0;
int offset_AC3 = 0;
int offset_DC  = 0;

void configurarSensoresCorrente() {
    analogReadResolution(12);
    pinMode(PIN_AC1, INPUT_ANALOG);
    pinMode(PIN_AC2, INPUT_ANALOG);
    pinMode(PIN_AC3, INPUT_ANALOG);
    pinMode(PIN_DC, INPUT_ANALOG);
}

// --- FUNÇÃO DE CALIBRAÇÃO (TARA) ---
void calibrarZeroCorrente() {
    long somaAC1 = 0;
    long somaAC2 = 0;
    long somaAC3 = 0;
    long somaDC  = 0;
    int n = 500; // Média de 500 leituras para estabilidade

    delay(100); // Aguarda estabilização da fonte

    for(int i=0; i<n; i++) {
        somaAC1 += analogRead(PIN_AC1);
        somaAC2 += analogRead(PIN_AC2);
        somaAC3 += analogRead(PIN_AC3);
        somaDC  += analogRead(PIN_DC);
        delay(1);
    }

    offset_AC1 = somaAC1 / n;
    offset_AC2 = somaAC2 / n;
    offset_AC3 = somaAC3 / n;
    offset_DC  = somaDC  / n;

    // --- DEBUG PARA VERIFICAÇÃO ---
    // IMPORTANTE:
    // Se VCC do Sensor = 5V: Offset esperado ~3100 (2.5V na entrada do STM32)
    // Se VCC do Sensor = 3.3V: Offset esperado ~2048 (1.65V)
    #if defined(USBCON)
      SerialUSB.println("--- CALIBRACAO CORRENTE (ACS758-100B) ---");
      SerialUSB.print("Offset AC1: "); SerialUSB.println(offset_AC1);
      SerialUSB.print("Offset AC2: "); SerialUSB.println(offset_AC2);
      SerialUSB.print("Offset AC3: "); SerialUSB.println(offset_AC3);
      SerialUSB.print("Offset DC:  "); SerialUSB.println(offset_DC);
      SerialUSB.print("Sensibilidade (V/A): "); SerialUSB.println(SENSIBILIDADE_V_A, 4);
      
      // Aviso de segurança para voltagem
      if(offset_AC1 > 3500) {
         SerialUSB.println("ALERTA: Offset muito alto! Risco de saturacao (clipping).");
      }
    #else
      Serial.println("--- CALIBRACAO CORRENTE (ACS758-100B) ---");
      Serial.print("Offset AC1: "); Serial.println(offset_AC1);
      Serial.print("Offset AC2: "); Serial.println(offset_AC2);
      Serial.print("Offset AC3: "); Serial.println(offset_AC3);
      Serial.print("Offset DC:  "); Serial.println(offset_DC);
      Serial.print("Sensibilidade (V/A): "); Serial.println(SENSIBILIDADE_V_A, 4);
    #endif
}

float rawToVoltage(int rawValue) {
    return (rawValue / ADC_SCALE) * ADC_VOLTAGE_REF;
}

DadosCorrente lerSensoresCorrente() {
    DadosCorrente dados;
    
    unsigned long startTime = millis();
    double somaQuadrada1 = 0;
    double somaQuadrada2 = 0;
    double somaQuadrada3 = 0;
    unsigned long amostrasCount = 0;

    // Janela de 60ms para capturar ciclos completos de rede (60Hz ou 50Hz)
    while (millis() - startTime < 60) {
        int raw1 = analogRead(PIN_AC1);
        int raw2 = analogRead(PIN_AC2);
        int raw3 = analogRead(PIN_AC3);

        // Subtrai offset para centralizar em 0 e converte para Volts
        float v1 = rawToVoltage(raw1 - offset_AC1); 
        float v2 = rawToVoltage(raw2 - offset_AC2);
        float v3 = rawToVoltage(raw3 - offset_AC3);

        somaQuadrada1 += (v1 * v1);
        somaQuadrada2 += (v2 * v2);
        somaQuadrada3 += (v3 * v3);
        
        amostrasCount++;
    }

    if (amostrasCount > 0) {
        // RMS = sqrt(Media(V^2)) / Sensibilidade
        dados.correnteAC1 = sqrt(somaQuadrada1 / amostrasCount) / SENSIBILIDADE_V_A;
        dados.correnteAC2 = sqrt(somaQuadrada2 / amostrasCount) / SENSIBILIDADE_V_A;
        dados.correnteAC3 = sqrt(somaQuadrada3 / amostrasCount) / SENSIBILIDADE_V_A;
    } else {
        dados.correnteAC1 = 0; dados.correnteAC2 = 0; dados.correnteAC3 = 0;
    }

    // Leitura DC (Média Simples)
    long somaDC = 0;
    for(int i=0; i<50; i++) { 
        somaDC += analogRead(PIN_DC);
    }
    float mediaRawDC = somaDC / 50.0;
    
    // Cálculo DC: (Tensão lida - Tensão de offset) / Sensibilidade
    float vDiffDC = rawToVoltage(mediaRawDC - offset_DC);
    dados.correnteDC = vDiffDC / SENSIBILIDADE_V_A;

    // Filtro de Zona Morta (Noise Gate)
    if (abs(dados.correnteAC1) < DEADZONE_A) dados.correnteAC1 = 0.0;
    if (abs(dados.correnteAC2) < DEADZONE_A) dados.correnteAC2 = 0.0;
    if (abs(dados.correnteAC3) < DEADZONE_A) dados.correnteAC3 = 0.0;
    if (abs(dados.correnteDC)  < DEADZONE_A) dados.correnteDC  = 0.0;

    return dados;
}