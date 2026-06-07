#ifndef HDWI_TYPE_H
#define HDWI_TYPE_H


#include <cstdint>

namespace godot {

    enum class HDWIType : uint8_t {
        GPIO    = 0,
        UART    = 1,
        I2C     = 2,
        SPI     = 3,
        ONEWIRE = 4,
        V4L2    = 5,
    };

}

#endif