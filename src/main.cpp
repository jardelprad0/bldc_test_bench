#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <STM32RTC.h>
#include "MedirCelulasdeCarga.h"
#include "MedirCorrente.h"
#include "MedirTensao.h"
#include "MedirRPM.h" // <--- INCLUÍDO AQUI

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
String formatFloat(float val, int casas); // Função auxiliar para formatar com vírgula

void setup() {
  MySerial.begin(115200);
  while (!MySerial && millis() < 4000) { ; }
  delay(1000);

  MySerial.println(">>> Monitoramento Completo: Fase, Total, Tensao, Potencia e RPM <<<");

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
    
    // Cabeçalho CSV Completo (Adicionado RPM)
    // Alterado para usar ponto e vírgula (;) como separador de colunas
    File dataFile = SD.open(nomeArquivoCSV.c_str(), FILE_WRITE);
    if (dataFile) {
      if (dataFile.size() == 0) {
        dataFile.println("Millis;Data;Torque(Nm);Thrust(Kg);AC1(A);AC2(A);AC3(A);DC_Total(A);Tensao(V);Potencia(W);RPM");
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

  MySerial.println("-> Sensor de RPM (Interrupt)...");
  configurarRPM(); // <--- CONFIGURAÇÃO RPM
  
  // Tara da Corrente (Zero)
  MySerial.println("-> Calibrando zero da corrente (Aguarde)...");
  calibrarZeroCorrente();
  
  MySerial.println("SISTEMA PRONTO.");
}

void loop() {
  processarCicloDeLeitura();
  // Taxa de atualização (o RPM calcula internamente a cada X ms, mas a exibição segue este delay)
  delay(250); 
}

void processarCicloDeLeitura() {
  
  // 1. LEITURA SINCRONIZADA DOS SENSORES
  DadosMedicao dadosCarga = lerDadosSensores();
  DadosCorrente dadosAmper = lerSensoresCorrente();
  DadosTensao dadosVolts  = lerSensorTensao();
  DadosRPM dadosRotacao   = lerRPM(); // <--- LEITURA RPM
  
  // 2. CÁLCULO DE POTÊNCIA REAL (HÍBRIDO)
  float potenciaReal_W = dadosVolts.tensaoV * dadosAmper.correnteDC;
  if (potenciaReal_W < 0) potenciaReal_W = 0;

  // 3. PLOTTER SERIAL (Usando vírgula)
  // Nota: O Serial Plotter padrão do Arduino pode não plotar gráficos corretamente com vírgula,
  // mas o Monitor Serial exibirá o texto conforme solicitado.
  MySerial.print(" Torque:");   MySerial.print(formatFloat(dadosCarga.torqueNm, 2));
  MySerial.print(" Thrust:");   MySerial.print(formatFloat(dadosCarga.thrustKg, 2));
  MySerial.print(" AC1:");      MySerial.print(formatFloat(dadosAmper.correnteAC1, 2));
  // MySerial.print(" AC2:");   MySerial.print(formatFloat(dadosAmper.correnteAC2, 2));
  // MySerial.print(" AC3:");   MySerial.print(formatFloat(dadosAmper.correnteAC3, 2));
  MySerial.print(" Total_DC:"); MySerial.print(formatFloat(dadosAmper.correnteDC, 2));
  MySerial.print(" Tensao:");   MySerial.print(formatFloat(dadosVolts.tensaoV, 2));
  MySerial.print(" Potencia_W:"); MySerial.print(formatFloat(potenciaReal_W, 2));
  MySerial.print(" RPM:");      MySerial.print(formatFloat(dadosRotacao.rpm, 0)); 
  
  MySerial.println(); 

  // 4. GRAVAÇÃO NO SD (CSV com ponto e vírgula e decimais com vírgula)
  if (SDok) {
    File dataFile = SD.open(nomeArquivoCSV.c_str(), FILE_WRITE);
    if (dataFile) {
      dataFile.print(millis());
      dataFile.print(";");
      dataFile.print(obterDataHoraFormatada());
      dataFile.print(";");
      // Mecânica
      dataFile.print(formatFloat(dadosCarga.torqueNm, 3));
      dataFile.print(";");
      dataFile.print(formatFloat(dadosCarga.thrustKg, 3));
      dataFile.print(";");
      // Elétrica (Fases)
      dataFile.print(formatFloat(dadosAmper.correnteAC1, 2));
      dataFile.print(";");
      dataFile.print(formatFloat(dadosAmper.correnteAC2, 2));
      dataFile.print(";");
      dataFile.print(formatFloat(dadosAmper.correnteAC3, 2));
      dataFile.print(";");
      // Elétrica (Entrada/Total)
      dataFile.print(formatFloat(dadosAmper.correnteDC, 2));
      dataFile.print(";");
      dataFile.print(formatFloat(dadosVolts.tensaoV, 2));
      dataFile.print(";");
      dataFile.print(formatFloat(potenciaReal_W, 2)); 
      dataFile.print(";");
      dataFile.println(formatFloat(dadosRotacao.rpm, 1)); 
      
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

// --- FUNÇÃO AUXILIAR DE FORMATAÇÃO ---
String formatFloat(float val, int casas) {
  String s = String(val, casas);
  s.replace('.', ','); // Substitui ponto por vírgula
  return s;
}