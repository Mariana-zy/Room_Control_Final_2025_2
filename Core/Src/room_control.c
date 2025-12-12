#include "room_control.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <string.h>
#include <stdio.h>

// Default password
static const char DEFAULT_PASSWORD[] = "2222";

// Temperature thresholds for automatic fan control
static const float TEMP_THRESHOLD_LOW = 25.0f;
static const float TEMP_THRESHOLD_MED = 28.0f;  
static const float TEMP_THRESHOLD_HIGH = 31.0f;

// Timeouts in milliseconds
static const uint32_t INPUT_TIMEOUT_MS = 10000;  // 10 seconds
static const uint32_t ACCESS_DENIED_TIMEOUT_MS = 3000;  // 3 seconds

// Control suave del ventilador con DMA (bonus) 
#define FAN_FADE_STEPS 20

static uint16_t fan_dma_buffer[FAN_FADE_STEPS];
static uint32_t fan_current_pwm_value = 0;
static uint8_t  fan_dma_in_progress = 0;

// Private function prototypes
static void room_control_change_state(room_control_t *room, room_state_t new_state);
static void room_control_update_display(room_control_t *room); 
static void room_control_update_door(room_control_t *room);
static void room_control_update_fan(room_control_t *room);
static fan_level_t room_control_calculate_fan_level(float temperature);
static void room_control_clear_input(room_control_t *room);

// Periféricos externos usados (*)
extern TIM_HandleTypeDef htim3;   // PWM TIM3 CH1 (PA6)
extern UART_HandleTypeDef huart3; // ESP-01 (USART3)

void room_control_init(room_control_t *room) {
    // Initialize room control structure
    room->current_state = ROOM_STATE_LOCKED;
    strcpy(room->password, DEFAULT_PASSWORD);
    room_control_clear_input(room);
    room->last_input_time = 0;
    room->state_enter_time = HAL_GetTick();
    
    // Initialize door control
    room->door_locked = true;
    
    // Initialize temperature and fan
    room->current_temperature = 22.0f;  // Default room temperature
    room->current_fan_level = FAN_LEVEL_OFF;
    room->manual_fan_override = false;
    
    // Display
    room->display_update_needed = true;
    // TODO: TAREA - Initialize hardware (door lock, fan PWM, etc.)  (*)
    
    // Ejemplo: HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    
    // Inicializar PWM en 0 (*)
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

    room_control_change_state(room, ROOM_STATE_LOCKED);
}

void room_control_update(room_control_t *room) {

    uint32_t current_time = HAL_GetTick();

    switch (room->current_state) {

        case ROOM_STATE_LOCKED:
            room->door_locked = true;
            break;

        case ROOM_STATE_INPUT_PASSWORD:
            if (current_time - room->last_input_time > INPUT_TIMEOUT_MS) {
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            break;

        case ROOM_STATE_UNLOCKED:
            room->door_locked = false;
            break;

        case ROOM_STATE_ACCESS_DENIED:
            if (current_time - room->state_enter_time > ACCESS_DENIED_TIMEOUT_MS) {
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            break;

        case ROOM_STATE_EMERGENCY:
            room->door_locked = false;
            room->current_fan_level = FAN_LEVEL_HIGH;
            break;
    }

    // Actualizar puerta física
    room_control_update_door(room);

    // Actualizar ventilador PWM
    room_control_update_fan(room);

    // Si hay cambios visuales, refrescar pantalla
    if (room->display_update_needed) {
        room_control_update_display(room);
        room->display_update_needed = false;
    }
}


void room_control_process_key(room_control_t *room, char key) {
    room->last_input_time = HAL_GetTick();
    
    switch (room->current_state) {
        case ROOM_STATE_LOCKED:
            // Start password input
            if ((key >= '0' && key <= '9') || key == 'C' || key == 'c') {  // aceptar solo números y borrar
                room_control_clear_input(room);
                room->input_buffer[0] = key;
                room->input_index = 1;
                room_control_change_state(room, ROOM_STATE_INPUT_PASSWORD);
            }
            break;
            
        case ROOM_STATE_INPUT_PASSWORD:
            // TODO: TAREA - Implementar lógica de entrada de teclas (*)

            // Tecla '*' = cancelar ingreso y volver a sistema bloqueado
            if (key == '*') {
                room_control_change_state(room, ROOM_STATE_LOCKED);
                break;
            }

            // Borrar ultimo digito ingresado 
            if (key == 'C' || key == 'c') { // borrar último dígito
                if (room->input_index > 0) {
                    room->input_index--;
                    room->input_buffer[room->input_index] = '\0';
                }

                // Actualizar display   
                room->display_update_needed = true;
                break;
            }

            // Solo se aceptan números
            if (key >= '0' && key <= '9') {
                
                // - Agregar tecla al buffer de entrada
                if (room->input_index < PASSWORD_LENGTH) {
                    room->input_buffer[room->input_index++] = key;
                }

                // - Verificar si se completaron 4 dígitos
                if (room->input_index == PASSWORD_LENGTH) {
                    
                    // - Comparar con contraseña guardada
                    if (strncmp(room->input_buffer, room->password, PASSWORD_LENGTH) == 0) {
                        // - Cambiar a UNLOCKED o ACCESS_DENIED según resultado
                        room_control_change_state(room, ROOM_STATE_UNLOCKED);
                    } else {
                        room_control_change_state(room, ROOM_STATE_ACCESS_DENIED);
                    }
                }
            }
            
            // Tecla '#' = confirmar contraseña si ya hay 4 dígitos
            if (key == '#') {
                if (room->input_index == PASSWORD_LENGTH) {
                    if (strncmp(room->input_buffer, room->password, PASSWORD_LENGTH) == 0) {
                        room_control_change_state(room, ROOM_STATE_UNLOCKED);
                    } else {
                        room_control_change_state(room, ROOM_STATE_ACCESS_DENIED);
                    }
                }
                room->display_update_needed = true;
                break;
            }

            // Actualizar display en cada tecla
            room->display_update_needed = true;

            break;
            
        case ROOM_STATE_UNLOCKED:
            // TODO: TAREA - Manejar comandos en estado desbloqueado (opcional)

            // Salir de modo manual y volver a automático
            if (key == 'A' || key == 'a') {
                room->manual_fan_override = false;
                room->current_fan_level = room_control_calculate_fan_level(room->current_temperature);
            }

            // Tecla 'B' para volver a bloquear
            else if (key == 'B' || key == 'b') {
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }

            // Control manual del ventilador
            else if (key == '0') { // Apagar ventilador
                room_control_force_fan_level(room, FAN_LEVEL_OFF);
            } else if (key == '1') { // Ventilador bajo
                room_control_force_fan_level(room, FAN_LEVEL_LOW);
            } else if (key == '2') { // Ventilador medio
                room_control_force_fan_level(room, FAN_LEVEL_MED);
            } else if (key == '3') { // Ventilador alto
                room_control_force_fan_level(room, FAN_LEVEL_HIGH);
            }

            // Entrar en modo EMERGENCIA (extra)
            else if (key == 'D' || key == 'd') {
                room_control_change_state(room, ROOM_STATE_EMERGENCY);
            }

            room->display_update_needed = true;

            break;
            
        case ROOM_STATE_EMERGENCY:
            if (key == '#') {   // tecla para salir de emergencia y IMPLEMENTAR entrar en emergencia 
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            break;
    }
            
    room->display_update_needed = true;
}   

void room_control_set_temperature(room_control_t *room, float temperature) {
    room->current_temperature = temperature;
    
    // Update fan level automatically if not in manual override
    if (!room->manual_fan_override) {
        fan_level_t new_level = room_control_calculate_fan_level(temperature);
        if (new_level != room->current_fan_level) {
            room->current_fan_level = new_level;
            room->display_update_needed = true;

            // Debug: cambio de nivel de ventilador en modo AUTO
            printf("AUTO: temp=%.1f -> nivel=%d\r\n",
                   temperature, new_level);
        }
    }
}   

void room_control_force_fan_level(room_control_t *room, fan_level_t level) {
    room->manual_fan_override = true;
    room->current_fan_level = level;
    room->display_update_needed = true;

    // Debug: forzado manual de ventilador
    printf("MANUAL: nivel=%d\r\n", level);
}

void room_control_change_password(room_control_t *room, const char *new_password) {
    if (strlen(new_password) == PASSWORD_LENGTH) {
        strcpy(room->password, new_password);
    }
}

// Status getters
room_state_t room_control_get_state(room_control_t *room) {
    return room->current_state;
}

bool room_control_is_door_locked(room_control_t *room) {
    return room->door_locked;
}

fan_level_t room_control_get_fan_level(room_control_t *room) {
    return room->current_fan_level;
}

float room_control_get_temperature(room_control_t *room) {
    return room->current_temperature;
}

// Para debug: convertir estado a string
static const char* room_state_to_str(room_state_t state) {
    switch (state) {
        case ROOM_STATE_LOCKED:         return "LOCKED";
        case ROOM_STATE_UNLOCKED:       return "UNLOCKED";
        case ROOM_STATE_INPUT_PASSWORD: return "INPUT_PASSWORD";
        case ROOM_STATE_ACCESS_DENIED:  return "ACCESS_DENIED";
        case ROOM_STATE_EMERGENCY:      return "EMERGENCY";
        default:                        return "UNKNOWN";
    }
}

// Private functions
static void room_control_change_state(room_control_t *room, room_state_t new_state) {

    // Debug: log de cambio de estado
    printf("Estado -> %s (temp=%.1f, fan=%d, manual=%d)\r\n",
           room_state_to_str(new_state),
           room->current_temperature,
           room->current_fan_level,
           room->manual_fan_override);

    room->current_state = new_state;
    room->state_enter_time = HAL_GetTick();
    room->display_update_needed = true;
    
    // State entry actions
    switch (new_state) {
        case ROOM_STATE_LOCKED:
            room->door_locked = true;
            room_control_clear_input(room);
            break;
            
        case ROOM_STATE_UNLOCKED:
            room->door_locked = false;
            room->manual_fan_override = false;  // Reset manual override
            break;

        // Presion de botón en una emergencia (extra)
        case ROOM_STATE_ACCESS_DENIED:
            room_control_clear_input(room);
            break;
            
        default:
            break;
    }
}

static void room_control_update_display(room_control_t *room) {

    ssd1306_Fill(Black);

    switch (room->current_state) {

        case ROOM_STATE_LOCKED:
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("SISTEMA", Font_11x18, White);
            ssd1306_SetCursor(10, 30);
            ssd1306_WriteString("BLOQUEADO", Font_11x18, White);
            break;

        case ROOM_STATE_INPUT_PASSWORD: {
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("CLAVE:", Font_11x18, White);

            char stars[PASSWORD_LENGTH + 1];
            for (int i = 0; i < PASSWORD_LENGTH; i++)
                stars[i] = (i < room->input_index) ? '*' : '_';

            stars[PASSWORD_LENGTH] = '\0';

            ssd1306_SetCursor(10, 35);
            ssd1306_WriteString(stars, Font_11x18, White);
            break;
        }

        case ROOM_STATE_UNLOCKED: {
            ssd1306_SetCursor(5, 0);
            ssd1306_WriteString("ACCESO OK", Font_11x18, White);

            char temp_buffer[32];
            snprintf(temp_buffer, sizeof(temp_buffer),
                     "Temp: %.1fC", room->current_temperature);
            ssd1306_SetCursor(5, 20);
            ssd1306_WriteString(temp_buffer, Font_11x18, White);

            const char *fan_str =
                (room->current_fan_level == FAN_LEVEL_OFF) ? "Vent: OFF" :
                (room->current_fan_level == FAN_LEVEL_LOW) ? "Vent: BAJO" :
                (room->current_fan_level == FAN_LEVEL_MED) ? "Vent: MEDIO" :
                                                             "Vent: ALTO";

            ssd1306_SetCursor(5, 40);
            ssd1306_WriteString(fan_str, Font_11x18, White);

            const char *mode_str =
                room->manual_fan_override ? "Modo: MANUAL" : "Modo: AUTO";

            ssd1306_SetCursor(10, 55);
            ssd1306_WriteString(mode_str, Font_11x18, White);
            break;
        }

        case ROOM_STATE_ACCESS_DENIED:
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("ACCESO", Font_11x18, White);
            ssd1306_SetCursor(10, 30);
            ssd1306_WriteString("DENEGADO", Font_11x18, White);
            break;

        case ROOM_STATE_EMERGENCY:
            ssd1306_SetCursor(0, 10);
            ssd1306_WriteString("EMERGENCIA!", Font_11x18, White);
            ssd1306_SetCursor(0, 35);
            ssd1306_WriteString("SALGA", Font_11x18, White);
            break;
    }

    ssd1306_UpdateScreen();
}

static void room_control_update_door(room_control_t *room) {
    // TODO: TAREA - Implementar control físico de la puerta
    // Ejemplo usando el pin DOOR_STATUS:
    if (room->door_locked) {
        // HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    } else {
        // HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    }
}

static void room_control_update_fan(room_control_t *room) {
    // TODO: TAREA - Implementar control PWM del ventilador
    // Calcular valor PWM basado en current_fan_level
    // Ejemplo:
    // uint32_t pwm_value = (room->current_fan_level * 99) / 100;  // 0-99 para period=99
    uint32_t pwm_value = 0;
    switch (room->current_fan_level) {
        case FAN_LEVEL_OFF:
            pwm_value = 0;
            break;
        case FAN_LEVEL_LOW:
            pwm_value = 30; // 30% duty cycle
            break;
        case FAN_LEVEL_MED:
            pwm_value = 70; // 70% duty cycle
            break;
        case FAN_LEVEL_HIGH:
            pwm_value = 99; // 100% duty cycle
            break;
        default:
            pwm_value = 0; 
            break;
    }

    // Si no hay cambio, solo asegurar el valor y salir
    if (pwm_value == fan_current_pwm_value) {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_value); // Aplicar valor PWM
        return;
    }

    // Si el DMA no está listo, caer a modo directo (sin fade)
    if (htim3.hdma[TIM_DMA_ID_CC1] == NULL ||
        htim3.hdma[TIM_DMA_ID_CC1]->State != HAL_DMA_STATE_READY) {

        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_value); // Aplicar valor PWM directo
        fan_current_pwm_value = pwm_value;
        return;
    }

    // Preparar buffer de transición suave entre el valor actual y el nuevo
    uint32_t start = fan_current_pwm_value;
    int32_t delta = (int32_t)pwm_value - (int32_t)start;

    for (int i = 0; i < FAN_FADE_STEPS; i++) {
        // i+1 para que el último valor sea exactamente pwm_value
        fan_dma_buffer[i] = (uint16_t)(start + (delta * (i + 1) / FAN_FADE_STEPS));
    }

    fan_current_pwm_value = pwm_value;
    fan_dma_in_progress = 1;

    // Iniciar transferencia DMA sobre TIM3 CH1
    if (HAL_TIM_PWM_Start_DMA(&htim3,
                              TIM_CHANNEL_1,
                              (uint32_t *)fan_dma_buffer,
                              FAN_FADE_STEPS) != HAL_OK) {

        // Si falla, desactivar el flag y aplicar el valor directo
        fan_dma_in_progress = 0;
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_value);
    }
}

static fan_level_t room_control_calculate_fan_level(float temperature) {
    // TODO: TAREA - Implementar lógica de niveles de ventilador

    // Seguridad: si llega un número fuera de rango extremo, evitar comportamientos raros
    if (temperature < -20.0f || temperature > 80.0f) {
        return FAN_LEVEL_OFF;
    }
    if (temperature < TEMP_THRESHOLD_LOW) {  
        return FAN_LEVEL_OFF;
    } else if (temperature < TEMP_THRESHOLD_MED) {
        return FAN_LEVEL_LOW;
    } else if (temperature < TEMP_THRESHOLD_HIGH) {
        return FAN_LEVEL_MED;
    } else {
        return FAN_LEVEL_HIGH;
    }
}

static void room_control_clear_input(room_control_t *room) {
    memset(room->input_buffer, 0, sizeof(room->input_buffer));
    room->input_index = 0;
}

// Callback de HAL para cuando termina una transferencia PWM por DMA
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) {
        // Detener DMA para este canal
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);

        // Marcar que ya no hay transición en curso
        fan_dma_in_progress = 0;

        // Asegurar que el CCR queda exactamente en el valor objetivo final
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, fan_current_pwm_value);
    }
}