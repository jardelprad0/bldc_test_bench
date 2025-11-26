#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

// Se a USB nativa estiver ativa (definida no platformio.ini), usa SerialUSB
// Caso contrário, usa a Serial padrão (UART)
#if defined(USBCON)
  #define MySerial SerialUSB
#else
  #define MySerial Serial
#endif

// Configuração dos pinos SPI conforme o tutorial:
// https://ardupiclab.blogspot.com/2021/08/how-to-use-sd-card-of-stm32f407vet6.html?m=1
//
// IMPORTANTE: Você precisa conectar fisicamente os pinos com jumpers (fios):
// PA7 (MOSI) -> PD2
// PA6 (MISO) -> PC8
// PA5 (SCK)  -> PC12
// PC11 (CS)  -> Já conectado internamente ao slot SD

#define SD_MOSI PA7
#define SD_MISO PA6
#define SD_CLK  PA5
#define SD_CS   PC11

bool SDok = false;

void setup() {
  MySerial.begin(115200);
  while (!MySerial) {
    ; // Aguarda conexão serial (necessário para USB nativa)
  }
  delay(2000); // Pequeno delay para garantir que o monitor serial pegue o início
  MySerial.println("Iniciando teste do SD Card (Modo SPI)...");

  // Configura os pinos originais do SDIO como INPUT_PULLUP para evitar interferência
  // conforme sugerido no tutorial.
  pinMode(PD2, INPUT_PULLUP);  // SDIO D0
  pinMode(PC8, INPUT_PULLUP);  // SDIO D1
  pinMode(PC12, INPUT_PULLUP); // SDIO CLK
  
  // Configura a instância SPI para usar os pinos definidos
  SPI.setMOSI(SD_MOSI);
  SPI.setMISO(SD_MISO);
  SPI.setSCLK(SD_CLK);

  MySerial.print("Inicializando SD card... ");

  // Inicializa o SD com o pino CS definido
  if (!SD.begin(SD_CS)) {
    MySerial.println("Falha na inicialização!");
    MySerial.println("Verifique as conexões dos jumpers:");
    MySerial.println("PA7 -> PD2");
    MySerial.println("PA6 -> PC8");
    MySerial.println("PA5 -> PC12");
    SDok = false;
    return;
  }
  
  MySerial.println("Inicialização concluída.");
  SDok = true;

  if (SDok) {
    MySerial.println("Criando arquivo de teste 'teste.txt'...");
    
    // Remove o arquivo se já existir para começar limpo
    if (SD.exists("teste.txt")) {
      SD.remove("teste.txt");
    }

    File myFile = SD.open("teste.txt", FILE_WRITE);

    if (myFile) {
      MySerial.print("Escrevendo no arquivo...");
      myFile.println("Teste de escrita no SD Card STM32F407VET6 - Sucesso!");
      myFile.close();
      MySerial.println("Feito.");
    } else {
      MySerial.println("Erro ao abrir teste.txt para escrita.");
    }

    // Re-abre o arquivo para leitura
    myFile = SD.open("teste.txt");
    if (myFile) {
      MySerial.println("--- Conteúdo de teste.txt ---");
      while (myFile.available()) {
        MySerial.write(myFile.read());
      }
      MySerial.println("\n--- Fim do arquivo ---");
      myFile.close();
    } else {
      MySerial.println("Erro ao abrir teste.txt para leitura.");
    }
  }
}

void loop() {
  // Nada a fazer no loop
  delay(1000);
}