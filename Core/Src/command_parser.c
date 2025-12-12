#include "command_parser.h"
#include "room_control.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Declaraciones externas de los UARTs definidos en main.c
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

// Usamos la instancia global definida en main.c
extern room_control_t room_system;

// Buffers independientes para debug (USART2) y ESP-01 (USART3)
#define CMD_BUFFER_SIZE 32

static char debug_buf[CMD_BUFFER_SIZE];
static uint8_t debug_idx = 0;

static char esp01_buf[CMD_BUFFER_SIZE];
static uint8_t esp01_idx = 0;

// Helpers privados

static void handle_command(const char *cmd, UART_HandleTypeDef *huart)
{
    // Eliminar posibles '\r' o espacios al final
    char local[CMD_BUFFER_SIZE];
    strncpy(local, cmd, CMD_BUFFER_SIZE);
    local[CMD_BUFFER_SIZE - 1] = '\0';
    size_t len = strlen(local);
    while (len > 0 && (local[len-1] == '\r' || local[len-1] == ' ')) {
        local[--len] = '\0';
    }

    // GET_TEMP
    if (strcmp(local, "GET_TEMP") == 0) {
        float t = room_control_get_temperature(&room_system);
        printf("TEMP: %.2f C\r\n", t);
        return;
    }

    // GET_STATUS
    if (strcmp(local, "GET_STATUS") == 0) {
        room_state_t st = room_control_get_state(&room_system);
        fan_level_t fan = room_control_get_fan_level(&room_system);
        uint8_t door_locked = room_control_is_door_locked(&room_system);

        printf("STATUS: state=%d, fan=%d, door_locked=%d\r\n",
               (int)st, (int)fan, (int)door_locked);
        return;
    }

    // FORCE_FAN:N
    if (strncmp(local, "FORCE_FAN:", 10) == 0) {
        char n = local[10];
        if (n >= '0' && n <= '3') {
            fan_level_t level = FAN_LEVEL_OFF;
            if (n == '1') level = FAN_LEVEL_LOW;
            else if (n == '2') level = FAN_LEVEL_MED;
            else if (n == '3') level = FAN_LEVEL_HIGH;

            room_control_force_fan_level(&room_system, level);
            printf("OK: FAN=%c\r\n", n);
        } else {
            printf("ERR: FORCE_FAN arg\r\n");
        }
        return;
    }

    // SET_PASS:XXXX
    if (strncmp(local, "SET_PASS:", 9) == 0) {
        const char *pass = &local[9];
        if (strlen(pass) == PASSWORD_LENGTH &&
            isdigit((unsigned char)pass[0]) &&
            isdigit((unsigned char)pass[1]) &&
            isdigit((unsigned char)pass[2]) &&
            isdigit((unsigned char)pass[3])) {

            room_control_change_password(&room_system, pass);
            printf("OK: PASS=%s\r\n", pass);
        } else {
            printf("ERR: PASS\r\n");
        }
        return;
    }

    // Si no coincide con nada
    printf("ERR: UNKNOWN CMD (%s)\r\n", local);
}

// API pública 

void command_parser_process_debug(uint8_t byte)
{
    if (byte == '\n') {
        debug_buf[debug_idx] = '\0';
        if (debug_idx > 0) {
            handle_command(debug_buf, &huart2);
        }
        debug_idx = 0;
        return;
    }

    if (debug_idx < CMD_BUFFER_SIZE - 1) {
        debug_buf[debug_idx++] = (char)byte;
    }
}

void command_parser_process_esp01(uint8_t byte)
{
    if (byte == '\n') {
        esp01_buf[esp01_idx] = '\0';
        if (esp01_idx > 0) {
            handle_command(esp01_buf, &huart3);
        }
        esp01_idx = 0;
        return;
    }

    if (esp01_idx < CMD_BUFFER_SIZE - 1) {
        esp01_buf[esp01_idx++] = (char)byte;
    }
}
