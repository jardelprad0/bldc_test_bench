#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <STM32RTC.h>
#include "MedirCelulasdeCarga.h"
#include "MedirCorrente.h"
#include "MedirTensao.h"
#include "MedirRPM.h"
#include "ControlarMotor.h" // Biblioteca ajustada com sua lógica

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

// Controle do Botão (Debounce)
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// --- PROTÓTIPOS ---
void processarCicloDeLeitura();
void lerBotaoFisico();
void configurarRTCAutomaticamente();
String obterDataHoraFormatada();
String formatFloat(float val, int casas);

void setup() {
  MySerial.begin(115200);
  while (!MySerial && millis() < 4000) { ; }
  delay(1000);

  MySerial.println(">>> Monitoramento Completo <<<");
  MySerial.println(">>> Aperte o botao em PE3 para iniciar a sequencia <<<");

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
    
    // Cabeçalho CSV (Adicionado Estado Motor)
    File dataFile = SD.open(nomeArquivoCSV.c_str(), FILE_WRITE);
    if (dataFile) {
      if (dataFile.size() == 0) {
        dataFile.println("Millis;Data;Torque(Nm);Thrust(Kg);AC1(A);AC2(A);AC3(A);DC_Total(A);Tensao(V);Potencia(W);RPM;Motor(%);Estado_Motor");
      }
      dataFile.close();
    }
  }

  // 3. Inicializar Sensores e Motor
  MySerial.println("-> Celulas de Carga...");
  configurarCelulas(); 

  MySerial.println("-> Sensores de Corrente (ACS758)...");
  configurarSensoresCorrente();
  
  MySerial.println("-> Sensor de Tensao (INA219)...");
  configurarSensorTensao();

  MySerial.println("-> Sensor de RPM...");
  configurarRPM();
  
  MySerial.println("-> Controle de Motor (PE3=Botao, PD13=PWM)...");
  configurarMotor(); 
  
  // Tara da Corrente
  MySerial.println("-> Calibrando zero da corrente...");
  calibrarZeroCorrente();

  // Agora pedimos os valores para imprimir aqui na Main
  MySerial.println("SISTEMA PRONTO.");
}

void loop() {
  lerBotaoFisico(); // Lê o botão PE3
  processarCicloDeLeitura();
  // Delay pequeno para visualização, mas a lógica do motor roda "dentro" do processar via millis
  delay(100); 
}

void lerBotaoFisico() {
  // Leitura do botão com Debounce
  // Assumindo INPUT_PULLUP (LOW = Pressionado)
  int reading = digitalRead(PINO_BOTAO_START);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Se o estado estabilizou e é LOW (apertou)
    // Precisamos de uma flag estática ou global para detectar a borda de descida apenas uma vez
    static int buttonState = HIGH;
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        MySerial.println(">>> BOTAO PRESSIONADO: Alternando Sequencia <<<");
        alternarEstadoSequencia();
      }
    }
  }
  lastButtonState = reading;
}

void processarCicloDeLeitura() {
  
  // 1. LÓGICA DO MOTOR (STATE MACHINE)
  DadosMotor dadosMot = processarMotor();
  
  // 2. LEITURA DE SENSORES
  DadosMedicao dadosCarga = lerDadosSensores();
  DadosCorrente dadosAmper = lerSensoresCorrente();
  DadosTensao dadosVolts  = lerSensorTensao();
  DadosRPM dadosRotacao   = lerRPM();
  
  // 3. CÁLCULO DE POTÊNCIA
  float potenciaReal_W = dadosVolts.tensaoV * dadosAmper.correnteDC;
  if (potenciaReal_W < 0) potenciaReal_W = 0;
  // 4. PLOTTER SERIAL
  //MySerial.print(" Torque:");   MySerial.print(formatFloat(dadosCarga.torqueNm, 2));
 // MySerial.print(" Thrust:");   MySerial.print(formatFloat(dadosCarga.thrustKg, 2));
  MySerial.print(" AC1:");      MySerial.print(formatFloat(dadosAmper.correnteAC1, 2));
  MySerial.print(" AC3:");      MySerial.print(formatFloat(dadosAmper.correnteAC3, 2));
  MySerial.print(" DC:");       MySerial.print(formatFloat(dadosAmper.correnteDC, 2));
  MySerial.print(" Tensao:");   MySerial.print(formatFloat(dadosVolts.tensaoV, 2));
  MySerial.print(" Potencia_W:"); MySerial.print(formatFloat(potenciaReal_W, 2));
  // MySerial.print(" RPM:");      MySerial.print(formatFloat(dadosRotacao.rpm, 0)); 
  // MySerial.print(" Motor_%:");  MySerial.print(formatFloat(dadosMot.aceleracaoPercentual, 1));
  // MySerial.print(" Est:");      MySerial.print(dadosMot.estadoAtual);
  
  
  MySerial.println(); 

  // 5. GRAVAÇÃO NO SD
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
      // Elétrica
      dataFile.print(formatFloat(dadosAmper.correnteAC1, 2));
      dataFile.print(";");
      dataFile.print(formatFloat(dadosAmper.correnteAC2, 2));
      dataFile.print(";");
      dataFile.print(formatFloat(dadosAmper.correnteAC3, 2));
      dataFile.print(";");
      dataFile.print(formatFloat(dadosAmper.correnteDC, 2));
      dataFile.print(";");
      dataFile.print(formatFloat(dadosVolts.tensaoV, 2));
      dataFile.print(";");
      dataFile.print(formatFloat(potenciaReal_W, 2)); 
      dataFile.print(";");
      dataFile.print(formatFloat(dadosRotacao.rpm, 1)); 
      dataFile.print(";");
      dataFile.print(formatFloat(dadosMot.aceleracaoPercentual, 1)); 
      dataFile.print(";");
      dataFile.println(dadosMot.estadoAtual);
      
      dataFile.close();
    }
  }
}

// --- FUNÇÕES AUXILIARES ---
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

String formatFloat(float val, int casas) {
  String s = String(val, casas);
  s.replace('.', ','); 
  return s;
}