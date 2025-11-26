#include "MedirCelulasdeCarga.h"
#include "HX711.h"

// --- PINOS DAS CÉLULAS DE CARGA (STM32 - Port C) ---
const int DOUT_PIN_torque = PC0;
const int SCK_PIN_torque  = PC1;
const int DOUT_PIN_thrust = PC2; //Cinza
const int SCK_PIN_thrust  = PC3; // branco

// --- OBJETOS HX711 (Locais apenas a este arquivo) ---
HX711 scale_torque;
HX711 scale_thrust;

// --- CONSTANTES DE FÍSICA E CALIBRAÇÃO ---
float calibration_factor_torque = -44.2960; 
float calibration_factor_thrust = -43.6067;
const float GRAMAS_PARA_NEWTONS = 0.009807;

// Distâncias (Metros)
const float distancia_x_torque = 0.195; 
const float distancia_y_torque = 0.100; 
const float distancia_x_thrust = 0.270; 
const float distancia_y_thrust = 0.140; 

// --- FUNÇÃO DE CONFIGURAÇÃO (SETUP) ---
void configurarCelulas() {
    // Configura Torque
    scale_torque.begin(DOUT_PIN_torque, SCK_PIN_torque);
    scale_torque.set_scale(calibration_factor_torque);
    scale_torque.tare(); 

    // Configura Thrust
    scale_thrust.begin(DOUT_PIN_thrust, SCK_PIN_thrust);
    scale_thrust.set_scale(calibration_factor_thrust);
    scale_thrust.tare();
}

// --- FUNÇÃO DE LEITURA (RETORNA ESTRUTURA COM DADOS) ---
DadosMedicao lerDadosSensores() {
    DadosMedicao dados;

    // 1. LEITURA E CÁLCULO DO TORQUE
    // Lê 5 amostras para média
    float leitura_em_gramas_torque = scale_torque.get_units(5); 
    float forca_celula_N_torque = leitura_em_gramas_torque * GRAMAS_PARA_NEWTONS;
    
    // Preenche dados de torque
    dados.rawTorqueG = leitura_em_gramas_torque;
    dados.torqueNm = forca_celula_N_torque * distancia_x_torque;

    // 2. LEITURA E CÁLCULO DO THRUST
    float leitura_em_gramas_thrust = scale_thrust.get_units(5);
    float fm = (distancia_y_thrust / distancia_x_thrust);
    
    // Preenche dados de thrust
    dados.rawThrustG = leitura_em_gramas_thrust;
    dados.thrustKg = fm * (leitura_em_gramas_thrust / 1000.0);

    return dados;
}