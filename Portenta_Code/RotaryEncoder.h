#ifndef ROTARYENCODER_H
#define ROTARYENCODER_H

#include <Arduino.h>

class RotaryEncoder {
public:
    RotaryEncoder(uint8_t pinA, uint8_t pinB);
    void begin();
    void update();
    long getPosition();
    void setPosition(long position);

private:
    static void handleInterruptA();
    static void handleInterruptB();

    static RotaryEncoder* instance;

    uint8_t _pinA, _pinB;
    volatile long _position;
    volatile bool _lastA;
};

#endif
