#ifndef HDWI_H
#define HDWI_H

#include <cstdint>



namespace godot {
    typedef uint8_t hdwi_type_t;

    #define HDWI_TYPE_GPIO ((hdwi_type)0x00)
    #define HDWI_TYPE_UART ((hdwi_type)0x01)
    #define HDWI_TYPE_I2C  ((hdwi_type)0x02)
    #define HDWI_TYPE_SPI  ((hdwi_type)0x03)

    
}




#endif