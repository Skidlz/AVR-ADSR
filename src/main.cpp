#include <Arduino.h>
#include "adsr.h"
#include "MCP48CXB28.h" //DAC

//Pin numbers
#define GATE_PN 2
#define A_RATE_PN A0
#define D_RATE_PN A1
#define SUS_PN A2
#define R_RATE_PN A3

void pollingLoop();
uint16_t nonblockAnalogRead(uint8_t pin, void (*callback)());

envelope ADSR;
uint8_t tick = 0; //timing flag
uint8_t gateState = 0;

ISR(TIMER1_COMPA_vect) { tick = 1; }

void setup() {
    ADSR.setAttack(256); //default values
    ADSR.setDecay(256);
    ADSR.setSustain(0);
    ADSR.setRelease(512);

    ADSR.gateOn();

    MCP::init(); //start DAC

    //setup timer for 4kHz tick-------------------------------------------
    noInterrupts();
    TCCR1A = TCCR1B = TCNT1 = 0;
    TCCR1B = _BV(CS11) | _BV(CS10) | _BV(WGM12); //div 64 16MHz/64=250kHz
    OCR1A = (250000 / 4000) - 1; //4kHz
    bitSet(TIMSK1, OCIE1A);
    interrupts();
}

void loop() {
    //read out four pots, and update the ADSR
    ADSR.setAttack(nonblockAnalogRead(A_RATE_PN, pollingLoop));
    ADSR.setDecay(nonblockAnalogRead(D_RATE_PN, pollingLoop));
    ADSR.setSustain(nonblockAnalogRead(SUS_PN, pollingLoop));
    ADSR.setRelease(nonblockAnalogRead(R_RATE_PN, pollingLoop));
}

void pollingLoop() { //this runs while we wait for pot readings
    if (tick) { //wait for timer interrupt
        tick = 0;

        ADSR.step();
        MCP::write(0, MCP::WRITE, ADSR.output); //set DAC
    }

    //poll gate pin--------------------------------------------------------
    uint8_t pinReading = ~PIND & _BV(GATE_PN);
    if (pinReading != gateState) {
        (!pinReading) ? ADSR.gateOn() : ADSR.gateOff();

        gateState = pinReading;
    }
}

uint16_t nonblockAnalogRead(uint8_t pin, void (*callback)()) { //non-blocking version of analogRead()
    if (pin >= 14) pin -= 14; //allow for channel or pin numbers

    //set the analog reference (high two bits of ADMUX) and select the
    //channel (low 4 bits).  this also sets ADLAR (left-adjust result) to 0 (the default).
    uint8_t analog_reference = 1; //default
    ADMUX = (analog_reference << 6) | (pin & 0x07);

    bitSet(ADCSRA, ADSC); //start the conversion

    while (bit_is_set(ADCSRA, ADSC)) { //ADSC is cleared when the conversion finishes
        callback(); //run callback while we wait
    };

    uint8_t low = ADCL; //have to read ADCL first to "lock" ADCH result
    uint8_t high = ADCH;
    return (high << 8) | low; // combine the two bytes
}