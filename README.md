# BLDC Test Bench

Este projeto consiste em uma bancada de testes automatizada para caracterização de motores BLDC e hélices, utilizando um microcontrolador STM32F407VET6. O sistema controla o motor através de uma sequência de testes pré-definida e registra dados de empuxo, torque, tensão, corrente e RPM em um cartão SD.

## 📋 Funcionalidades

- **Controle Automático de Motor:** Executa uma rampa de aceleração escalonada (ex: degraus de 10% a cada 2 segundos).
- **Monitoramento de Sensores:**
  - **Tensão:** INA219 (via I2C).
  - **Corrente:** ACS758 (Analógico).
  - **Força (Empuxo/Torque):** Células de carga com HX711.
  - **RPM:** Sensor de rotação (Interrupção).
- **Data Logging:** Gravação de dados em cartão SD em formato CSV.
- **RTC:** Registro de data e hora para cada arquivo de log.
- **Interface:** Monitor Serial para debug e botão físico para início/parada.

## 🛠 Hardware

- **Microcontrolador:** STM32F407VET6 (Black Board)
- **Sensores:**
  - Módulo INA219 (Tensão/Corrente - usado aqui principalmente para Tensão)
  - Sensor de Corrente ACS758
  - Amplificadores HX711 (para Células de Carga)
  - Sensor de RPM (Óptico ou Hall)
- **Atuador:** ESC (Electronic Speed Controller) para motor BLDC
- **Armazenamento:** Módulo Cartão SD

## 🔌 Pinagem (Pinout)

| Periférico | Pino STM32 | Função |
| :--- | :--- | :--- |
| **Motor (ESC)** | `PD13` | Sinal PWM |
| **Botão Início** | `PE4` | Botão (Input Pullup) - Inicia/Para sequência |
| **Sensor RPM** | `PD12` | Entrada de Pulso (Interrupção) |
| **I2C (INA219)** | `PB6` | SCL |
| | `PB7` | SDA |
| **SD Card** | `PA7` | MOSI |
| | `PA6` | MISO |
| | `PA5` | CLK |
| | `PC11` | CS |
| **SD Detect** | `PD2`, `PC8`, `PC12` | Pinos de detecção (verificar esquema) |

> **Nota:** O código pode mencionar `PE3` em mensagens de log, mas a definição funcional está configurada para `PE4`.

## 🚀 Como Usar

1. **Montagem:** Conecte todos os sensores e o ESC conforme a pinagem acima.
2. **Cartão SD:** Insira um cartão SD formatado (FAT32).
3. **Energia:** Alimente a placa e o sistema de potência do motor.
4. **Início:**
   - O sistema iniciará e calibrará a corrente (tara).
   - Aguarde a mensagem "SISTEMA PRONTO" no Monitor Serial.
   - Pressione o botão conectado em **PE4**.
5. **Teste:**
   - O motor iniciará a sequência de aceleração (degraus).
   - Os dados serão mostrados no Serial e gravados no SD.
   - Ao final, o motor desliga automaticamente.

## 📂 Estrutura do Projeto

- `src/main.cpp`: Loop principal, gerenciamento do SD e orquestração.
- `src/ControlarMotor.cpp`: Máquina de estados para controle do PWM do motor.
- `src/MedirCelulasdeCarga.cpp`: Leitura dos sensores de força (HX711).
- `src/MedirCorrente.cpp`: Leitura do sensor ACS758.
- `src/MedirTensao.cpp`: Leitura do INA219.
- `src/MedirRPM.cpp`: Contagem de pulsos para cálculo de rotação.

## 📦 Dependências

As bibliotecas são gerenciadas automaticamente pelo PlatformIO (`platformio.ini`):

- `arduino-libraries/SD`
- `bogde/HX711`
- `stm32duino/STM32duino RTC`
- `adafruit/Adafruit INA219`

## ⚙️ Configuração

Para ajustar parâmetros como pinos, calibração de sensores ou curva de aceleração, edite os arquivos `.h` correspondentes na pasta `src/` ou as definições no topo dos arquivos `.cpp`.
