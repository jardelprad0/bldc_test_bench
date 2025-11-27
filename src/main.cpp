#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <STM32RTC.h> // Biblioteca do RTC
#include "MedirCelulasdeCarga.h"
#include "MedirCorrente.h" // Inclui a nossa nova biblioteca

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

  MySerial.println(">>> Iniciando Sistema: Carga + Corrente <<<");

  // 1. Inicializar RTC
  rtc.setClockSource(STM32RTC::LSE_CLOCK); 
  rtc.begin(); 

  // Diagnóstico do RTC
  if (rtc.getClockSource() == STM32RTC::LSE_CLOCK) {
    MySerial.println("RTC Clock: LSE (OK).");
  } else {
    MySerial.println("RTC Clock: LSI (AVISO: Clock interno).");
  }

  // Verificar se Data é válida
  uint8_t h, m, s, d, mo, y, wd;
  uint32_t ss;
  rtc.getDate(&wd, &d, &mo, &y);
  rtc.getTime(&h, &m, &s, &ss);
  
  if (!rtc.isTimeSet() || y < 25) {
    MySerial.println("Data invalida. Ajustando RTC...");
    configurarRTCAutomaticamente();
  }

  // Nome do Arquivo CSV (Data/Hora)
  char fileNameBuf[32];
  sprintf(fileNameBuf, "%02d%02d%02d%02d.csv", d, mo, h, m);
  nomeArquivoCSV = String(fileNameBuf);
  MySerial.print("Arquivo Log: "); MySerial.println(nomeArquivoCSV);

  // 2. Inicializar SD
  pinMode(PD2, INPUT_PULLUP);
  pinMode(PC8, INPUT_PULLUP);
  pinMode(PC12, INPUT_PULLUP);
  
  SPI.setMOSI(SD_MOSI);
  SPI.setMISO(SD_MISO);
  SPI.setSCLK(SD_CLK);

  if (!SD.begin(SD_CS)) {
    MySerial.println("ERRO: SD Card falhou!");
    SDok = false;
  } else {
    MySerial.println("SD Card OK.");
    SDok = true;
    
    // Cabeçalho CSV Completo
    File dataFile = SD.open(nomeArquivoCSV.c_str(), FILE_WRITE);
    if (dataFile) {
      if (dataFile.size() == 0) {
        dataFile.println("Millis,DataHora,Torque(Nm),Thrust(Kg),AC1(A),AC2(A),AC3(A),DC(A)");
      }
      dataFile.close();
    }
  }

  // 3. Inicializar Sensores de Carga
  MySerial.println("Configurando Celulas de Carga...");
  configurarCelulas(); 

  // 4. Inicializar Sensores de Corrente
  MySerial.println("Configurando Sensores de Corrente...");
  configurarSensoresCorrente();
  
  // --- PASSO CRÍTICO: CALIBRAÇÃO (TARA) ---
  MySerial.println("A CALIBRAR ZERO DE CORRENTE... (Nao ligue cargas agora)");
  calibrarZeroCorrente();
  MySerial.println("Calibracao Concluida.");

  MySerial.println("Sistema pronto.");
}

void loop() {
  processarCicloDeLeitura();
  // Delay reduzido pois a leitura AC já consome ~40ms
  delay(500); 
}

void processarCicloDeLeitura() {
  
  // 1. LER DADOS
  DadosMedicao dadosCarga = lerDadosSensores();
  DadosCorrente dadosAmper = lerSensoresCorrente();
  String dataHora = obterDataHoraFormatada();

  // 2. MOSTRAR NO TERMINAL
  MySerial.print("["); MySerial.print(dataHora); MySerial.println("]");
  
  MySerial.print(" Mecanica | Torque: ");
  MySerial.print(dadosCarga.torqueNm, 2);
  MySerial.print(" Nm | Thrust: ");
  MySerial.print(dadosCarga.thrustKg, 2);
  MySerial.println(" Kg");

  MySerial.print(" Eletrica | AC1: ");
  MySerial.print(dadosAmper.correnteAC1, 1);
  MySerial.print("A | AC2: ");
  MySerial.print(dadosAmper.correnteAC2, 1);
  MySerial.print("A | AC3: ");
  MySerial.print(dadosAmper.correnteAC3, 1);
  MySerial.print("A | DC: ");
  MySerial.print(dadosAmper.correnteDC, 1);
  MySerial.println("A");
  MySerial.println("-----------------------------");

  // 3. GRAVAR NO SD
  if (SDok) {
    File dataFile = SD.open(nomeArquivoCSV.c_str(), FILE_WRITE);
    if (dataFile) {
      dataFile.print(millis());
      dataFile.print(",");
      dataFile.print(dataHora);
      dataFile.print(",");
      dataFile.print(dadosCarga.torqueNm, 3);
      dataFile.print(",");
      dataFile.print(dadosCarga.thrustKg, 3);
      dataFile.print(",");
      // Dados Elétricos
      dataFile.print(dadosAmper.correnteAC1, 2);
      dataFile.print(",");
      dataFile.print(dadosAmper.correnteAC2, 2);
      dataFile.print(",");
      dataFile.print(dadosAmper.correnteAC3, 2);
      dataFile.print(",");
      dataFile.println(dadosAmper.correnteDC, 2);
      
      dataFile.close();
    } else {
      // Se falhar a abrir, tenta reiniciar o flag (opcional)
      // MySerial.println("Erro SD: Gravar."); 
    }
  }
}

// --- FUNÇÕES AUXILIARES RTC ---
String obterDataHoraFormatada() {
  uint8_t hours, minutes, seconds;
  uint32_t subSeconds;
  uint8_t day, month, year, weekDay;
  
  rtc.getTime(&hours, &minutes, &seconds, &subSeconds);
  rtc.getDate(&weekDay, &day, &month, &year);

  char buffer[25];
  sprintf(buffer, "20%02d-%02d-%02d %02d:%02d:%02d", year, month, day, hours, minutes, seconds);
  return String(buffer);
}

void configurarRTCAutomaticamente() {
  char s_month[5];
  int year, day, hour, minute, second;
  static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

  sscanf(__DATE__, "%s %d %d", s_month, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

  int month = (strstr(month_names, s_month) - month_names) / 3 + 1;
  int year2d = year - 2000;

  rtc.setTime(hour, minute, second);
  rtc.setDate(day, month, year2d);
}