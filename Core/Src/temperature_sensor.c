#include "temperature_sensor.h"
#include "main.h"   // Para hadc1 y HAL

extern ADC_HandleTypeDef hadc1;

/**
 * @brief Inicializa el módulo de lectura de temperatura.
 * 
 * El ADC ya está configurado en MX_ADC1_Init(), así que aquí
 * no necesitamos hacer nada adicional por ahora.
 */
void temperature_sensor_init(void)
{
    // ADC ya inicializado por MX_ADC1_Init()
}

/**
 * @brief Lee el sensor LM35 conectado al canal configurado del ADC1.
 * 
 * LM35: 10 mV/°C  →  0.01 V/°C
 * Vtemp = (adc_value / 4095) * Vref
 * Temp(°C) = Vtemp / 0.01 = Vtemp * 100
 * 
 * @return Temperatura en grados Celsius.
 */
float temperature_sensor_read(void)
{
    uint32_t adc_value = 0;

    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return 0.0f;
    }

    // Timeout pequeño para no bloquear el superloop
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return 0.0f;
    }

    adc_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    const float vref = 3.3f;
    float voltage = ((float)adc_value * vref) / 4095.0f;
    float temperature_c = voltage * 100.0f; // LM35: 10 mV/°C

    return temperature_c;
}
