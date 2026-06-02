#ifndef HDWI_SPI_H
#define HDWI_SPI_H

#include "../hdwi.h"
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <unordered_map>
#include <vector>

namespace godot {

    #define SPI_SETUP 0x01
    #define SPI_TRANSFER 0x02


    class HDWISPIResource : public HDWIResource {
        GDCLASS(HDWISPIResource, HDWIResource)
        private:
            // No member variables needed for now, but can be added later if necessary
            HDWIType type = HDWIType::SPI;

        protected:
            static void _bind_methods();
        
        public:

            static TypedArray<HDWISPIResource> spi_resources; // Static array to hold all SPI resources for group representation

            HDWISPIResource() = default;

            ~HDWISPIResource() = default;

            void init(){
                HDWISPIResource::spi_resources.append(this); // Add this instance to the static array of SPI resources
            }

            void clear() {
                HDWISPIResource::spi_resources.erase(this); // Remove this instance from the static array of SPI resources
            }

            // Signals
            // Component - (HDWI) -> CommSeq
            void on_send();

            // Bus index in @example variable, setter and getter
            uint8_t bus_index;
            void set_bus_index(uint8_t p_bus_index);
            uint8_t get_bus_index() const;

            // Speed in Hz @export variable, setter and getter
            uint32_t max_speed_hz;
            void set_max_speed_hz(uint32_t p_speed_hz);
            uint32_t get_max_speed_hz() const;

            // Mode (0-3) @export variable, setter and getter
            uint8_t mode;
            void set_mode(uint8_t p_mode);
            uint8_t get_mode() const;

            // Bits per word @export variable, setter and getter
            uint8_t bits_per_word;
            void set_bits_per_word(uint8_t p_bits_per_word);
            uint8_t get_bits_per_word() const;

            // Chip select line @export variable, setter and getter
            uint8_t chip_select_line;
            void set_chip_select_line(uint8_t p_chip_select_line);
            uint8_t get_chip_select_line() const;
            
            // Handlers
            void handle_spi_setup(PackedByteArray request_data);
            void handle_spi_transfer(PackedByteArray request_data);

            // Methods
            // Comm - (Device Registry -> Component's HDWI Resource) -> HDWI
            virtual void dispatch_action(PackedByteArray request_data) override;

            // Get all the devices of this type for group representation
            static PackedByteArray get_group_type_representation();
            
            PackedByteArray get_device_representation() const override;
    };
}


#endif 