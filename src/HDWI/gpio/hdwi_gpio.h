#ifndef HDWI_GPIO_H
#define HDWI_GPIO_H

#include "../hdwi.h"
#include "../../IPC/sim_packet_type.h"
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <unordered_map>

namespace godot {

    #pragma pack(push,1)
    struct GpioRequest {
        uint8_t action;   // GPIO_GET / GPIO_SET / GPIO_DIR_IN / GPIO_DIR_OUT
        uint8_t offset;   // line number, 0..ngpio-1
        uint8_t value;    // SET / DIR_OUT initial level (0/1); ignored otherwise
    };                    // 3 bytes
    struct GpioResponse {
        uint8_t status;   // 0 = OK
        uint8_t value;    // GET result; else 0
    };                    // 2 bytes
    #pragma pack(pop)


    // HDWIGPIOResource — one virtual GPIO chip (a bank of digital lines).
    //
    // Interface:
    //   1. set chip_index (wire routing id) and seed gpio_values (size = #lines)
    //   2. init() to register the chip
    //   3. connect on_gpio_line_change to observe lines the FSW drives;
    //      connect on_send (base) to ship GPIO_GET responses to the socket
    //   4. to present an input level to the FSW, assign gpio_values; the next
    //      GPIO_GET returns the current level at that offset
    //
    // Actions handled (see sim_packet_type.h): GPIO_GET / GPIO_SET /
    // GPIO_DIR_IN / GPIO_DIR_OUT. Full reference: doc_classes/HDWIGPIOResource.xml
    class HDWIGPIOResource : public HDWIResource {
        GDCLASS(HDWIGPIOResource, HDWIResource)
        private:
            HDWIType type = HDWIType::GPIO;
            PackedInt32Array gpio_dirs;

        protected:
            static void _bind_methods();
            static TypedArray<HDWIGPIOResource> *gpio_resources;
            static std::unordered_map<uint8_t, int> chip_index_to_device_id;

        public:
            uint8_t chip_index = 0;
            void set_chip_index(int p_chip_index);
            int  get_chip_index() const;

            HDWIGPIOResource() = default;
            ~HDWIGPIOResource() = default;

            virtual void init() override {
                if (gpio_resources == nullptr) {
                    gpio_resources = new TypedArray<HDWIGPIOResource>();
                }
                gpio_resources->append(this);
                chip_index_to_device_id[chip_index] = gpio_resources->size() - 1;
            }

            virtual void clear() override {
                gpio_resources->erase(this);
            }

            void on_send();
            void on_gpio_line_change(uint8_t line_offset, uint8_t new_value);

            virtual void dispatch_action(PackedByteArray request_data) override;

            PackedInt32Array gpio_values;
            void set_gpio_values(PackedInt32Array p_gpio_values);
            PackedInt32Array get_gpio_values() const;

            // When true (default), an engine-driven change to an INPUT line (via
            // set_gpio_values) automatically raises a GPIO_IRQ_LINE_CHANGE so the
            // FSW's registered line-event handler fires — modelling a real edge
            // interrupt. Changes the FSW drives itself (GPIO_SET on its own outputs)
            // never produce an IRQ. Set false to fall back to poll-only (GPIO_GET).
            bool irq_enabled = true;
            void set_irq_enabled(bool p_enabled);
            bool get_irq_enabled() const;

            // Explicit edge injection: raise GPIO_IRQ_LINE_CHANGE for `line` at
            // `value` regardless of direction or irq_enabled. Use when you want to
            // pulse an interrupt without staging it through gpio_values.
            void send_line_change_irq(int line, int value);

            static PackedByteArray get_group_type_representation();
            PackedByteArray get_device_representation() const override;

            // Returns 0-based index in gpio_resources for the chip identified by id;
            // -1 if not registered.
            static int gpio_dev_id_to_device_id(gpio_dev_id_t id);
            // GDScript-callable variant.
            static int lookup_gpio_device_id(int p_chip_index);

            void handle_gpio_get(GpioRequest request);
            void handle_gpio_set(GpioRequest request);
            void handle_gpio_dir_out(GpioRequest request);
            void handle_gpio_dir_in(GpioRequest request);
    };

}


#endif