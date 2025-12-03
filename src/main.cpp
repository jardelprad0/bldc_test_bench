#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <STM32RTC.h>
#include "MedirCelulasdeCarga.h"
#include "MedirCorrente.h"
#include "MedirTensao.h"

// --- CONFIGURAÇÃO SERIAL E SD ---
#if defined(USBCON)
  #define MySerial SerialUSB
#else
  #define MySerial Serial
#endif

// Pinos SPI do SD Card (STM32F407)
#define SD_MOSI PA7
#define SD_MISO PA6
#define SD_CLK  PA5
#define SD_CS   PC11

bool SDok = false;
String nomeArquivoCSV = "dados.csv";

// --- OBJETOS GLOBAIS ---
STM32RTC& rtc = STM32RTC::getInstance();

// --- PROTÓTIPOS ---
void processarCicloDeLeitura();
void configurarRTCAutomaticamente();
String obterDataHoraFormatada();

void setup() {
  MySerial.begin(115200);
  while (!MySerial && millis() < 4000) { ; }
  delay(1000);

  MySerial.println(">>> Monitoramento Completo: Fase, Total, Tensao e Potencia <<<");

  // 1. Inicializar RTC
  rtc.setClockSource(STM32RTC::LSE_CLOCK); 
  rtc.begin(); 

  // Verificar Data e Hora
  uint8_t wd, d, mo, y;
  rtc.getDate(&wd, &d, &mo, &y);
  if (!rtc.isTimeSet() || y < 25) {
    configurarRTCAutomaticamente();
  }

  // Nome do Arquivo de Log
  uint8_t h, m, s;
  uint32_t ss;
  rtc.getTime(&h, &m, &s, &ss);
  char fileNameBuf[32];
  sprintf(fileNameBuf, "%02d%02d%02d%02d.csv", d, mo, h, m);
  nomeArquivoCSV = String(fileNameBuf);
  MySerial.print("Log criado: "); MySerial.println(nomeArquivoCSV);

  // 2. Inicializar SD
  pinMode(PD2, INPUT_PULLUP);
  pinMode(PC8, INPUT_PULLUP);
  pinMode(PC12, INPUT_PULLUP);
  SPI.setMOSI(SD_MOSI); SPI.setMISO(SD_MISO); SPI.setSCLK(SD_CLK);

  if (!SD.begin(SD_CS)) {
    MySerial.println("ERRO: Falha no SD Card!");
    SDok = false;
  } else {
    MySerial.println("SD Card OK.");
    SDok = true;
    
    // Cabeçalho CSV Completo
    File dataFile = SD.open(nomeArquivoCSV.c_str(), FILE_WRITE);
    if (dataFile) {
      if (dataFile.size() == 0) {
        dataFile.println("Millis,Data,Torque(Nm),Thrust(Kg),AC1(A),AC2(A),AC3(A),DC_Total(A),Tensao(V),Potencia(W)");
      }
      dataFile.close();
    }
  }

  // 3. Inicializar Sensores
  MySerial.println("-> Celulas de Carga...");
  configurarCelulas(); 

  MySerial.println("-> Sensores de Corrente (ACS758)...");
  configurarSensoresCorrente();
  
  MySerial.println("-> Sensor de Tensao (INA219)...");
  configurarSensorTensao();
  
  // Tara da Corrente (Zero)
  MySerial.println("-> Calibrando zero da corrente (Aguarde)...");
  calibrarZeroCorrente();
  
  MySerial.println("SISTEMA PRONTO.");
}

void loop() {
  processarCicloDeLeitura();
  // Taxa de atualização (ajuste conforme necessário para o Serial Plotter não engasgar)
  delay(250); 
}

void processarCicloDeLeitura() {
  
  // 1. LEITURA SINCRONIZADA DOS SENSORES
  DadosMedicao dadosCarga = lerDadosSensores();
  DadosCorrente dadosAmper = lerSensoresCorrente();
  DadosTensao dadosVolts  = lerSensorTensao();
  
  // 2. CÁLCULO DE POTÊNCIA REAL (HÍBRIDO)
  // Potência (W) = Tensão Medida (INA219) * Corrente Total DC (ACS758)
  float potenciaReal_W = dadosVolts.tensaoV * dadosAmper.correnteDC;

  // Evitar potência negativa por ruído de corrente zero
  if (potenciaReal_W < 0) potenciaReal_W = 0;

  // 3. PLOTTER SERIAL (Formato Label:Valor para Arduino Plotter)
  // Exibe todas as correntes de fase, a DC total, Tensão e Potência
  MySerial.print(" Torque:"); MySerial.print(dadosCarga.torqueNm, 2);
  MySerial.print(" Thrust:"); MySerial.print(dadosCarga.thrustKg, 2);
  MySerial.print(" AC1:"); MySerial.print(dadosAmper.correnteAC1, 2);
  MySerial.print(" AC2:"); MySerial.print(dadosAmper.correnteAC2, 2);
  MySerial.print(" AC3:"); MySerial.print(dadosAmper.correnteAC3, 2);
  MySerial.print(" Total_DC:"); MySerial.print(dadosAmper.correnteDC, 2);
  MySerial.print(" Tensao:");   MySerial.print(dadosVolts.tensaoV, 2);
  MySerial.print(" Potencia_W:"); MySerial.print(potenciaReal_W, 2);
  
  // Se quiser ver Torque/Thrust no plotter também, descomente abaixo:

  
  MySerial.println(); // Fim da linha para o Plotter

  // 4. GRAVAÇÃO NO SD (CSV)
  if (SDok) {
    File dataFile = SD.open(nomeArquivoCSV.c_str(), FILE_WRITE);
    if (dataFile) {
      dataFile.print(millis());
      dataFile.print(",");
      dataFile.print(obterDataHoraFormatada());
      dataFile.print(",");
      // Mecânica
      dataFile.print(dadosCarga.torqueNm, 3);
      dataFile.print(",");
      dataFile.print(dadosCarga.thrustKg, 3);
      dataFile.print(",");
      // Elétrica (Fases)
      dataFile.print(dadosAmper.correnteAC1, 2);
      dataFile.print(",");
      dataFile.print(dadosAmper.correnteAC2, 2);
      dataFile.print(",");
      dataFile.print(dadosAmper.correnteAC3, 2);
      dataFile.print(",");
      // Elétrica (Entrada/Total)
      dataFile.print(dadosAmper.correnteDC, 2);
      dataFile.print(",");
      dataFile.print(dadosVolts.tensaoV, 2);
      dataFile.print(",");
      dataFile.println(potenciaReal_W, 2); 
      
      dataFile.close();
    }
  }
}

// --- FUNÇÕES AUXILIARES RTC ---
String obterDataHoraFormatada() {
  uint8_t h, m, s, d, mo, y, wd;
  uint32_t ss;
  rtc.getTime(&h, &m, &s, &ss);
  rtc.getDate(&wd, &d, &mo, &y);
  char buffer[25];
  sprintf(buffer, "20%02d-%02d-%02d %02d:%02d:%02d", y, mo, d, h, m, s);
  return String(buffer);
}

void configurarRTCAutomaticamente() {
  char s_month[5];
  int year, day, hour, minute, second;
  static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  sscanf(__DATE__, "%s %d %d", s_month, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
  int month = (strstr(month_names, s_month) - month_names) / 3 + 1;
  rtc.setTime(hour, minute, second);
  rtc.setDate(day, month, year - 2000);
}