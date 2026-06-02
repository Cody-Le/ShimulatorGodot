#ifndef HDWI_GPIO_H
#define HDWI_GPIO_H

#include "../hdwi.h"
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

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


    typedef uint8_t gpio_action_t;
    // GPIO action constants
    #define GPIO_GET       ((gpio_action_t)0x01)
    #define GPIO_SET       ((gpio_action_t)0x02)
    #define GPIO_DIR_OUT   ((gpio_action_t)0x03)
    #define GPIO_DIR_IN    ((gpio_action_t)0x04)

    typedef struct sim_gpio_request {
        gpio_action_t action;
        uint8_t offset;
        uint8_t value;  // for set and dir_out
    } sim_gpio_request_t;

    typedef struct sim_gpio_response {
        uint8_t value;  // for get and dir_in
    } sim_gpio_response_t;


    class HDWIGPIOResource : public HDWIResource {
        GDCLASS(HDWIGPIOResource, HDWIResource)
        private:
            // No member variables needed for now, but can be added later if necessary
            HDWIType type = HDWIType::GPIO;
            // PackedInt32Array to hold the dir of each lines
            PackedInt32Array gpio_dirs; // 0 for input, 1 for output
            // PackedInt32Array to hold the value of each lines

        protected:
            static void _bind_methods();
        
        public:

            static TypedArray<HDWIGPIOResource> gpio_resources; // Static array to hold all GPIO resources for group representation


            HDWIGPIOResource() = default;

            ~HDWIGPIOResource() = default;

            void init(){
                HDWIGPIOResource::gpio_resources.append(this); // Add this instance to the static array of GPIO resources
            }

            void clear() {
                // Remove this instance from the static array of GPIO resources upon destruction
                HDWIGPIOResource::gpio_resources.erase(this);
            }
            // Signals
            // Component - (HDWI) -> CommSeq

            void on_send();
            //GPIO line changes signals
            void on_gpio_line_change(uint8_t line_offset, uint8_t new_value);
            
            // Methods
            // Comm - (Device Registry -> Component's HDWI Resource) -> HDWI
            virtual void dispatch_action(PackedByteArray request_data) override;

            // @export array of values, representing line offset and their values.
            // Its length the ngpio.
            PackedInt32Array gpio_values;
            void set_gpio_values(PackedInt32Array p_gpio_values);
            PackedInt32Array get_gpio_values() const;

            // Get all the devices of this type for group representation
            static TypedArray<HDWIGPIOResource> get_all_gpio_resources();

            // Get device representation as a packed byte array
            PackedByteArray get_device_representation() const override;


            // GPIO Handler functions
            void handle_gpio_get(sim_gpio_request_t request);
            void handle_gpio_set(sim_gpio_request_t request);
            void handle_gpio_dir_out(sim_gpio_request_t request);
            void handle_gpio_dir_in(sim_gpio_request_t request);

    };

}


#endif