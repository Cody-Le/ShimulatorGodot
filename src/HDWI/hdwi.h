#ifndef HDWI_H
#define HDWI_H

#include <cstdint>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/resource.hpp>



namespace godot {

    enum class HDWIType : uint8_t {
        GPIO = 0,
        UART = 1,
        I2C = 2,
        SPI = 3,
    };

    class HDWIResource : public Resource {
        GDCLASS(HDWIResource, Resource)
        private:
            // No member variables needed for now, but can be added later if necessary
        
        protected:
            static void _bind_methods();
        
        public:
            HDWIResource() = default;

    };
}




#endif