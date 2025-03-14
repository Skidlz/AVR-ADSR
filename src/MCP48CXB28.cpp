#include <SPI.h>
#include <MCP48CXB28.h>

namespace MCP {
    void init() {
        pinMode(CS_PN, OUTPUT); //set the CS pin as an output
        bitSet(PORTB, CS_PN - 8);
        SPI.begin();
        SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE2));

        write(8, WRITE, 0b0101010101010101); //VREF register. use bandgap
        write(10, WRITE, 0b11111111 << 7); //Gain register. 2x
    }

    //address 0-7 are DAC channels
    uint16_t write(uint8_t address, uint8_t command, uint16_t data) {
        bitClear(PORTB, CS_PN - 8);
        SPI.transfer(((address & 0b11111) << 3) | ((command & 0b11) << 1));
        uint16_t response = SPI.transfer16(data);
        bitSet(PORTB, CS_PN - 8);
        return response;
    }
}