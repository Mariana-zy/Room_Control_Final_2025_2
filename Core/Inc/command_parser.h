#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <stdint.h>

void command_parser_process_debug(uint8_t byte);

// Versión para ESP-01 (nivel intermedio)
void command_parser_process_esp01(uint8_t byte);

#endif // COMMAND_PARSER_H
