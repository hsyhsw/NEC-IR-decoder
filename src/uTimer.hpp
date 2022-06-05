#ifndef __uTimer_hpp__
#define __uTimer_hpp__

#include <Arduino.h>

class uTimer {
    public:
        void start(unsigned long duration) {
            this->duration = duration;
            this->_start();
        }

        unsigned long elapsed() {
            return micros() - this->initial;
        }

        bool expired() {
            return this->initial + this->duration < micros();
        }

    private:
        unsigned long initial;
        unsigned long duration;

        void _start() {
            this->initial = micros();
        }
};

#endif // __uTimer_hpp__
