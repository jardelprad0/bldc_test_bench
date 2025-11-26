#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "MedirCelulasdeCarga.h" // Inclui nosso novo módulo

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

// --- PROTÓTIPO DA FUNÇÃO AUXILIAR DESTA MAIN ---
void processarCicloDeLeitura();

void setup() {
  MySerial.begin(115200);
  while (!MySerial && millis() < 4000) { ; }
  delay(1000);

  MySerial.println(">>> Iniciando Sistema Modularizado <<<");

  // 1. Inicializar Hardware SD
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
    
    // Cabeçalho CSV
    File dataFile = SD.open("dados.csv", FILE_WRITE);
    if (dataFile) {
      if (dataFile.size() == 0) {
        dataFile.println("Torque(Nm),LeituraTorque(g),Forca(Kg),LeituraThrust(g)");
      }
      dataFile.close();
    }
  }

  // 2. Inicializar Módulo de Células
  MySerial.println("Calibrando celulas...");
  configurarCelulas(); // Chama a função do outro arquivo

  MySerial.println("Sistema pronto.");
}

void loop() {
  processarCicloDeLeitura();
  delay(1000); 
}

// --- FUNÇÃO INTEGRADORA (Lê do módulo -> Mostra -> Grava) ---
void processarCicloDeLeitura() {
  
  // 1. CHAMA O MÓDULO PARA OBTER DADOS
  // Aqui a mágica acontece: pegamos todos os dados calculados de uma vez
  DadosMedicao dados = lerDadosSensores();

  // 2. MOSTRAR NO TERMINAL
  MySerial.print("Torque: ");
  MySerial.print(dados.torqueNm, 3);
  MySerial.print(" N.m | Raw: ");
  MySerial.print(dados.rawTorqueG, 1);
  MySerial.print(" g || ");
  
  MySerial.print("Thrust: ");
  MySerial.print(dados.thrustKg, 3);
  MySerial.print(" Kg | Raw: ");
  MySerial.print(dados.rawThrustG, 1);
  MySerial.println(" g");
  MySerial.println("--------------------------------------------------");

  // 3. GRAVAR NO SD
  if (SDok) {
    File dataFile = SD.open("dados.csv", FILE_WRITE);
    if (dataFile) {
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