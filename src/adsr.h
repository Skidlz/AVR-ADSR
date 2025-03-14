#pragma once

#include <stdint.h>
#include <avr/pgmspace.h>

class envelope {
public:
    int16_t output = 0; //0-4095

    void gateOn();
    void gateOff();
    void setAttack(uint16_t); //0-1023
    void setDecay(uint16_t);
    void setSustain(uint16_t);
    void setRelease(uint16_t);
    void step();

private:
    static constexpr uint8_t INT_BITS = 12; //whole numbers
    static constexpr uint8_t FRAC_BITS = 8; //decimal digits
    static constexpr uint32_t MAX_CNT = (((uint32_t)1 << INT_BITS) << FRAC_BITS);
    static constexpr int16_t MAX_OUT = ((uint32_t)1 << INT_BITS) - 1;
    enum Stages : uint8_t { STOPPED, ATTACK, DECAY, SUSTAIN, RELEASE };

    Stages stage = STOPPED;
    uint32_t progress = 0; //0 - MAX_CNT, tracks progress through a stage
    uint32_t a,d,r; //rates, added to 'progress' once per step() to advance the stage
    int16_t s; //sustain level
    uint16_t releaseStart = 0; //0 - MAX_OUT

    typedef struct { uint16_t slope; int16_t offset; } piecewiseFunc;
    static const piecewiseFunc expoAttack[8] PROGMEM ;
    static const piecewiseFunc expoDecay[8] PROGMEM ;

    typedef struct { uint16_t max; uint16_t slope; int16_t offset; } invPiecewiseFunc;
    //use to convert from output back to attack progress when restarting envelope
    static const invPiecewiseFunc inverseAttack[8] PROGMEM;

    uint16_t expoConvert(const piecewiseFunc[], uint16_t);
    uint16_t invExpoConvert(uint16_t);
};