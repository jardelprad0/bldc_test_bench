# BLDC Test Bench

This project is an automated test bench for BLDC motor and propeller characterization using an STM32F407VET6 microcontroller. The system drives the motor through a predefined test sequence and logs thrust, torque, voltage, current, and RPM data to an SD card.

## 📋 Features

- **Automatic Motor Control:** Runs a step-based acceleration ramp (for example, 10% duty-cycle steps every 2 seconds).
- **Sensor Monitoring:**
  - **Voltage:** INA219 (I2C).
  - **Current:** ACS758 (analog).
  - **Force (Thrust/Torque):** Load cells with HX711 amplifiers.
  - **RPM:** SNG-QPLA-000 speed sensor (interrupt-based).
- **Data Logging:** Records measurements to CSV files on an SD card.
- **RTC Timestamping:** Includes date/time information in log filenames.
- **Interface:** Serial monitor for debug and a physical button to start/stop tests.

## 🛠 Hardware

- **Microcontroller:** STM32F407VET6 (Black Board)
- **Sensors:**
  - INA219 module (Voltage/Current - primarily used here for voltage)
  - ACS758 current sensor
  - HX711 amplifiers (for load cells)
  - Honeywell SNG-QPLA-000 speed sensor (Quadrature Speed and Direction Sensor)
- **Actuation:** ESC (Electronic Speed Controller) for BLDC motor
- **Storage:** SD card module

## 🔌 Pinout

| Peripheral | STM32 Pin | Function |
| :--- | :--- | :--- |
| **Motor (ESC)** | `PD13` | PWM signal |
| **Start Button** | `PE4` | Button (Input Pullup) - Start/Stop sequence |
| **RPM Sensor** | `PD12` | Pulse input (interrupt) |
| **I2C (INA219)** | `PB6` | SCL |
| | `PB7` | SDA |
| **SD Card** | `PA7` | MOSI |
| | `PA6` | MISO |
| | `PA5` | CLK |
| | `PC11` | CS |
| **SD Detect** | `PD2`, `PC8`, `PC12` | Detection pins (check wiring/schematic) |

> **Note:** The code may reference `PE3` in log messages, but the functional button pin is configured as `PE4`.

## 🚀 How to Use

1. **Assembly:** Connect all sensors and the ESC according to the pinout above.
2. **SD Card:** Insert a FAT32-formatted SD card.
3. **Power:** Supply both the board and the motor power stage.
4. **Start:**
   - The system boots and performs current offset calibration (tare).
   - Wait for the "SISTEMA PRONTO" message on the serial monitor.
   - Press the button connected to **PE4**.
5. **Test Run:**
   - The motor executes the acceleration step sequence.
   - Data is printed to serial and saved to SD.
   - At the end of the sequence, the motor is turned off automatically.

## 📂 Project Structure

- `src/main.cpp`: Main loop, SD management, and orchestration.
- `src/ControlarMotor.cpp`: State machine for motor PWM control.
- `src/MedirCelulasdeCarga.cpp`: Load-cell (HX711) reading routines.
- `src/MedirCorrente.cpp`: ACS758 current measurement.
- `src/MedirTensao.cpp`: INA219 voltage measurement.
- `src/MedirRPM.cpp`: Pulse counting and RPM calculation.

## 📈 Experimental Results

The following plots summarize the recorded datasets in `Experiments/air` and `Experiments/water`.

### Air Tests - 2312 Motor

![Air 2312 analysis](Experiments/images/2312_en.png)

### Air Tests - 3508 Motor

![Air 3508 analysis](Experiments/images/m3508_en.png)

### Air Tests - 3508 Failed Run

![Air 3508 failed run analysis](Experiments/images/fail_m3504.png)

### Water Tests - T200

![Water T200 analysis](Experiments/images/t200_en.png)

## 📦 Dependencies

Libraries are managed automatically by PlatformIO in `platformio.ini`:

- `arduino-libraries/SD`
- `bogde/HX711`
- `stm32duino/STM32duino RTC`
- `adafruit/Adafruit INA219`

## ⚙️ Configuration

To adjust parameters such as pin mapping, sensor calibration, or acceleration profile settings, edit the corresponding `.h` files in `src/` or the top-level definitions in each `.cpp` file.

## 📚 How to Cite

If you use this project in academic work, please cite it as:

```bibtex
@INPROCEEDINGS{11565558,
  author={Prado, Jardel J. P. and Pinheiro, Pedro M. and Junior, Adir A. P. and Estrada, Emanuel S. D. and Drews-Jr, Paulo L. J.},
  booktitle={2026 Brazilian Conference on Robotics (CROS)}, 
  title={Propulsion of Hybrid Aerial-Underwater Vehicles: Experimental Setup and Dataset}, 
  year={2026},
  volume={},
  number={},
  pages={549-554},
  keywords={Motors;Testing;Modeling;Propulsion;Propellers;Torque;Printing;Measurement;Equations;Media;Dataset;Propulsion Characterization;Bimodal Test Bench},
  doi={10.1109/CROS69211.2026.11565558}}
```
