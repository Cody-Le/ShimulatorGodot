#ifndef HDWI_GPIO_H
#define HDWI_GPIO_H

#include "../hdwi.h"

namespace godot {

    #pragma pack(push,1)
    struct GpioRequest {
        uint8_t action;   // SET / GET / DIR_IN / DIR_OUT / GET_DIR
        uint8_t offset;   // line number, 0..ngpio-1
        uint8_t value;    // SET / DIR_OUT initial level (0/1); ignored otherwise
    };                    // 3 bytes
    struct GpioResponse {
        uint8_t status;   // 0 = OK
        uint8_t value;    // GET / GET_DIR result; else 0
    };                    // 2 bytes
    #pragma pack(pop)


    class HDWIGPIOResource : public HDWIResource {
        GDCLASS(HDWIGPIOResource, HDWIResource)
        private:
            // No member variables needed for now, but can be added later if necessary
            HDWIType type = HDWIType::GPIO;

        protected:
            static void _bind_methods();
        
        public:
            // Signals
            // Component - (HDWI) -> CommSeq
            void on_send();
          
            
            // Methods
            // Comm - (Device Registry -> Component's HDWI Resource) -> HDWI
            void dispatch_action(PackedByteArray request_data);

            // @export variables, setters and getters: num_lines
            uint8_t num_lines = 32; // Default to 32 lines, can be changed by user
            void set_num_lines(uint8_t p_num_lines);
            uint8_t get_num_lines() const;

            // base: for a base N, the device name will be listed as gpiochipN
            // @export variables, setters and getters: base
            uint8_t base = 0; // Default to base 0, can be changed by user
            void set_base(uint8_t p_base);
            uint8_t get_base() const;

            // Get device representation as a packed byte array
            PackedByteArray get_device_representation();
            
            HDWIGPIOResource() = default;

    };

}


#endif