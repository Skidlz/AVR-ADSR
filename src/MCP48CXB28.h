#pragma once

#include <stdint.h>

namespace MCP {
    constexpr uint8_t WRITE = 0;
    constexpr uint8_t READ = 4;
    constexpr uint8_t CS_PN = 10;
    void init();
    uint16_t write(uint8_t address, uint8_t command, uint16_t data);
}