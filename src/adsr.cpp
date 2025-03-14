#include "adsr.h"

void envelope::gateOn() {
    stage = ATTACK;
    //reset attack progress to equivalent output
    progress = (uint32_t)invExpoConvert(output) << FRAC_BITS;
}

void envelope::gateOff() {
    releaseStart = output; //start release from whatever point we're at
    progress = MAX_CNT;
    stage = RELEASE;
}

void envelope::setAttack(uint16_t rate) { //0-1023
    if (rate > 1023) rate = 1023;
    //based around a hyperbolic curve function: (1-x)/(cx+1). c sets the quality of the curve

    // max count = 4096, sample rate = 4000, prescale = 256, pot max = 1024, max time = 3 seconds, c = 1536
    //period in seconds = (max count / ((cx / max time) / (pot max - x) + max count * prescale / max time / sample rate)) / sample rate
    // (4096 / ((1536 * x / 3) / (1024 - x) + 4096 * 256 / 3 / 4000)) / 4000
    // 512 * x / (1024 - x) + 87.38... = 512 * 1024 / (1024 - x) - 424.62
    a = ((uint32_t)1 << 19) / (1024 - rate) - 425; //3 - .5m seconds. Calculation takes ~600 cycles
}

void envelope::setDecay(uint16_t rate) {
    if (rate > 1023) rate = 1023;
    //c = 1600, m(ax) = 10 seconds
    // (cx / m) / (1024 - x) + 4096 * 256 / m / 4000
    // 160 x / (1024 - x) + 26.2144 = 163840 / (x - 1024) - 133.786
    d = (163840) / (1024 - rate) - 134; //10 - 1m seconds
}

void envelope::setSustain(uint16_t level) {
    if (level > 1023) level = 1023;
    s = level * 4;
}

void envelope::setRelease(uint16_t rate) {
    if (rate > 1023) rate = 1023;
    r = (165600) / (1035 - rate) - 134; //10 - 2m seconds
}

void envelope::step() { //advance the envelope
    switch (stage) {
        case ATTACK:
            progress += a;
            output = expoConvert(expoAttack, progress >> FRAC_BITS);
            if (output > MAX_OUT) output = MAX_OUT;

            if (progress >= MAX_CNT) {
                progress = MAX_CNT;
                stage = DECAY;
            }
            break;
        case DECAY:
            progress -= d;
            output = expoConvert(expoDecay, progress >> FRAC_BITS);
            if (output > MAX_OUT) output = MAX_OUT;

            //scale and offset decay relative to sustain
            output = (((uint32_t)output * (MAX_OUT - s)) >> INT_BITS) + s;

            if (progress <= 0 || progress >= MAX_CNT || output <= s) { //check for underflow
                output = s;
                stage = SUSTAIN;
            }
            break;
        case SUSTAIN: //wait until release
            output = s; //update in case s changed
            break;
        case RELEASE:
            progress -= r;
            output = expoConvert(expoDecay, progress >> FRAC_BITS);

            //scale based on amplitude the release started at
            output = ((uint32_t)output * releaseStart) >> INT_BITS;

            if (progress <=0 || output <= 0) {
                output = 0;
                stage = STOPPED;
            }
    }
}

const envelope::piecewiseFunc envelope::expoAttack[8] = { //array of lines approximating 0<x<8 y=(1-1/e^(x/2.6276)) * 1.05
        { 681, 0 }, { 466, 430 }, { 318, 1020 }, { 217, 1626 },
        { 149, 2174 }, { 102, 2644 }, { 70, 3028 }, { 48, 3336 }
};

const envelope::piecewiseFunc envelope::expoDecay[8] = { //array of lines approximating 0<x<8 y=(1-1/e^(x/1.441)) * 1.0039
        { 8, 0 }, { 16, -16 }, { 32, -80 }, { 64, -272 },
        { 129, -788 }, { 257, -2068 }, { 514, -5158 }, { 1029, -12368 }
};

const envelope::invPiecewiseFunc envelope::inverseAttack[8] = { //inverse of expoAttack
        { 1361, 96, 0 }, { 2292, 141, -236 },
        { 2928, 206, -821 }, { 3362, 302, -1918 },
        { 3659, 441, -3748 }, { 3862, 646, -6669 },
        { 4001, 943, -11153 }, { 4096, 1380, -17979 }
};

//convert from linear to piecewise approximation of an exponential curve
uint16_t envelope::expoConvert(const piecewiseFunc expoStruct[], uint16_t linVal) { //0 - 4096 value
    uint8_t index = linVal >> 9; //truncate to just 8 values
    if (index >= 8) index = 7;

    uint16_t slope = pgm_read_word_near(&expoStruct[index].slope); //load from flash
    int16_t offset = pgm_read_word_near(&expoStruct[index].offset);

    return (((uint32_t)linVal * slope) >> 8) + offset; //returns 0 - 4096
}

//work backwards from output value to progress value for attack
uint16_t envelope::invExpoConvert(uint16_t expoVal) { //0 - 4096 value
    uint8_t index = 7; //default to max
    for (uint8_t i = 0; i < 8; i++) {//find line segment to use
        if (pgm_read_word_near(&inverseAttack[i].max) > expoVal) {
            index = i;
            break;
        }
    }

    uint16_t slope = pgm_read_word_near(&inverseAttack[index].slope); //load from flash
    int16_t offset = pgm_read_word_near(&inverseAttack[index].offset);

    return (((uint32_t)expoVal * slope) >> 8) + offset; //returns 0 - 4096
}