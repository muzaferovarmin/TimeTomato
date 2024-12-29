#include "RotaryEncoder.h"

RotaryEncoder* RotaryEncoder::instance = nullptr;

RotaryEncoder::RotaryEncoder(uint8_t pinA, uint8_t pinB) {
    _pinA = pinA;
    _pinB = pinB;
    _position = 0;
    instance = this;
}

void RotaryEncoder::begin() {
    pinMode(_pinA, INPUT);
    pinMode(_pinB, INPUT);
    _lastA = digitalRead(_pinA);
    attachInterrupt(digitalPinToInterrupt(_pinA), handleInterruptA, CHANGE);
    attachInterrupt(digitalPinToInterrupt(_pinB), handleInterruptB, CHANGE);
}

void RotaryEncoder::update() {
    bool currentA = digitalRead(_pinA);
    bool currentB = digitalRead(_pinB);
    if (currentA != _lastA) {
        if (currentA == currentB) {
            _position++;
        } else {
            _position--;
        }
        _lastA = currentA;
    }
}

void RotaryEncoder::handleInterruptA() {
    instance->update();
}

void RotaryEncoder::handleInterruptB() {
    instance->update();
}

long RotaryEncoder::getPosition() {
    return _position;
}

void RotaryEncoder::setPosition(long position) {
    _position = position;
}
