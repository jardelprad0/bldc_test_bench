#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <STM32RTC.h> // Biblioteca do RTC
#include "MedirCelulasdeCarga.h"

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
/* Get the rtc object */
STM32RTC& rtc = STM32RTC::getInstance();

// --- PROTÓTIPOS ---
void processarCicloDeLeitura();
void configurarRTCAutomaticamente();
String obterDataHoraFormatada();

void setup() {
  MySerial.begin(115200);
  while (!MySerial && millis() < 4000) { ; }
  delay(1000);

  MySerial.println(">>> Iniciando Sistema Modularizado com RTC <<<");

  // 1. Inicializar RTC
  rtc.setClockSource(STM32RTC::LSE_CLOCK); // Tenta forçar LSE (Cristal Externo) para funcionar com bateria
  rtc.begin(); 

  // Diagnóstico do RTC no Boot
  if (rtc.getClockSource() == STM32RTC::LSE_CLOCK) {
    MySerial.println("RTC Clock: LSE (OK - Bateria deve funcionar).");
  } else {
    MySerial.println("RTC Clock: LSI (AVISO: Usando clock interno, hora vai parar ao desligar).");
  }

  // Ler data/hora atual para diagnóstico
  uint8_t h, m, s, d, mo, y, wd;
  uint32_t ss;
  rtc.getTime(&h, &m, &s, &ss);
  rtc.getDate(&wd, &d, &mo, &y);
  
  MySerial.print("Data/Hora lida do RTC no boot: 20");
  MySerial.print(y); MySerial.print("-"); MySerial.print(mo); MySerial.print("-"); MySerial.print(d);
  MySerial.print(" "); MySerial.print(h); MySerial.print(":"); MySerial.print(m); MySerial.print(":"); MySerial.println(s);

  // Lógica de ajuste:
  // Se o ano for < 25 (ex: 2000), assumimos que a bateria falhou ou é o primeiro uso.
  // Nesse caso, atualizamos para a data de compilação.
  if (!rtc.isTimeSet() || y < 25) {
    MySerial.println("Data invalida detectada (Bateria falhou?). Ajustando para hora da compilacao...");
    configurarRTCAutomaticamente();
  } else {
    MySerial.println("RTC parece valido. Mantendo hora atual.");
  }

  // Atualizar nome do arquivo com base na hora atual (seja ela lida ou ajustada)
  rtc.getTime(&h, &m, &s, &ss);
  rtc.getDate(&wd, &d, &mo, &y);
  char fileNameBuf[32];
  // Formato 8.3 (max 8 chars nome): DDMMHHMM.csv (Ex: 27110912.csv)
  // O formato anterior "Dados_..." era muito longo para o sistema de arquivos FAT padrão do Arduino
  sprintf(fileNameBuf, "%02d%02d%02d%02d.csv", d, mo, h, m);
  nomeArquivoCSV = String(fileNameBuf);
  MySerial.print("Arquivo de log definido: ");
  MySerial.println(nomeArquivoCSV);

  // 2. Inicializar Hardware SD
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
    
    // Cabeçalho CSV atualizado com DataHora e Millis
    File dataFile = SD.open(nomeArquivoCSV.c_str(), FILE_WRITE);
    if (dataFile) {
      if (dataFile.size() == 0) {
        // Adicionada a coluna Millis e DataHora no início
        dataFile.println("Millis,DataHora,Torque(Nm),LeituraTorque(g),Forca(Kg),LeituraThrust(g)");
      }
      dataFile.close();
    }
  }

  // 3. Inicializar Módulo de Células
  MySerial.println("Calibrando celulas...");
  configurarCelulas(); 

  MySerial.println("Sistema pronto.");
}

void loop() {
  processarCicloDeLeitura();
  delay(1000); 
}

// --- FUNÇÃO INTEGRADORA ---
void processarCicloDeLeitura() {
  
  // 1. OBTER DADOS E DATA/HORA
  DadosMedicao dados = lerDadosSensores();
  String dataHora = obterDataHoraFormatada();

  // 2. MOSTRAR NO TERMINAL
  MySerial.print("[");
  MySerial.print(dataHora);
  MySerial.print("] ");
  
  MySerial.print("Torque: ");
  MySerial.print(dados.torqueNm, 3);
  MySerial.print(" N.m | ");
  
  MySerial.print("Thrust: ");
  MySerial.print(dados.thrustKg, 3);
  MySerial.println(" Kg");

  // 3. GRAVAR NO SD
  if (SDok) {
    File dataFile = SD.open(nomeArquivoCSV.c_str(), FILE_WRITE);
    if (dataFile) {
      dataFile.print(millis()); // Grava os milissegundos desde o boot
      dataFile.print(",");
      dataFile.print(dataHora); // Grava a data e hora
      dataFile.print(",");
      dataFile.print(dados.torqueNm, 4);
      dataFile.print(",");
      dataFile.print(dados.rawTorqueG, 2);
      dataFile.print(",");
      dataFile.print(dados.thrustKg, 4);
      dataFile.print(",");
      dataFile.println(dados.rawThrustG, 2);
      
      dataFile.close();
    } else {
      MySerial.println("Erro SD: Abrir arquivo.");
    }
  }
}

// --- FUNÇÕES AUXILIARES RTC ---

// Retorna string no formato "DD/MM/YYYY HH:MM:SS"
String obterDataHoraFormatada() {
  // Ler hora e data do RTC
  uint8_t hours, minutes, seconds;
  uint32_t subSeconds;
  uint8_t day, month, year; // STM32RTC usa ano com 2 dígitos (ex: 23 para 2023)
  uint8_t weekDay;
  
  rtc.getTime(&hours, &minutes, &seconds, &subSeconds);
  rtc.getDate(&weekDay, &day, &month, &year);

  char buffer[25];
  // Formata: 2025-11-27 10:30:05 (Formato ISO facilita ordenar no Excel)
  sprintf(buffer, "20%02d-%02d-%02d %02d:%02d:%02d", year, month, day, hours, minutes, seconds);
  
  return String(buffer);
}

// Configura o RTC com base no momento da compilação do código
void configurarRTCAutomaticamente() {
  // Macros __DATE__ = "Mmm dd yyyy" (ex: "Nov 27 2025")
  // Macros __TIME__ = "hh:mm:ss" (ex: "10:30:05")
  char s_month[5];
  int year, day, hour, minute, second;
  
  // Mapeamento de meses
  static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

  sscanf(__DATE__, "%s %d %d", s_month, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

  // Converter nome do mês para número
  int month = (strstr(month_names, s_month) - month_names) / 3 + 1;
  
  // Ajustar ano para 2 dígitos (STM32RTC library standard)
  int year2d = year - 2000;

  MySerial.print("Configurando RTC para: ");
  MySerial.print(year); MySerial.print("-"); MySerial.print(month); MySerial.print("-"); MySerial.print(day);
  MySerial.print(" ");
  MySerial.print(hour); MySerial.print(":"); MySerial.print(minute); MySerial.print(":"); MySerial.println(second);

  rtc.setTime(hour, minute, second);
  rtc.setDate(day, month, year2d);
}