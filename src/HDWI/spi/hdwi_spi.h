#ifndef HDWI_SPI_H
#define HDWI_SPI_H

#include "../hdwi.h"
#include "../../IPC/sim_packet_type.h"
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <unordered_map>
#include <vector>

namespace godot {

    // HDWISPIResource — one SPI slave addressed by (bus_index, chip_select_line).
    //
    // SPI is full duplex: each transfer the FSW pushes N MOSI bytes and expects N
    // MISO bytes back in the same exchange.
    //
    // Interface:
    //   1. set bus_index, chip_select_line, mode, bits_per_word, max_speed_hz
    //      (mode/bpw/speed get overwritten when the FSW issues a SPI_SETUP)
    //   2. init() to register the device
    //   3. connect on_spi_transfer; in the handler compute the MISO reply and call
    //      set_miso_buffer() SYNCHRONOUSLY — the handler runs inline before the
    //      response is framed, so deferring it desyncs the stream. Wrong-length
    //      buffers are clamped/padded to the transfer size (default N zero bytes).
    //   4. connect on_spi_setup (optional) to observe bus configuration;
    //      connect on_send (base) to ship the MISO response to the socket
    //
    // Full reference: doc_classes/HDWISPIResource.xml
    class HDWISPIResource : public HDWIResource {
        GDCLASS(HDWISPIResource, HDWIResource)
        private:
            HDWIType type = HDWIType::SPI;

        protected:
            static void _bind_methods();
            static TypedArray<HDWISPIResource> *spi_resources;
            // Key: (bus_index << 8) | chip_select → 0-based index in spi_resources
            static std::unordered_map<uint16_t, int> bus_cs_to_device_id;

        
        public:


            HDWISPIResource() = default;

            ~HDWISPIResource() = default;

            virtual void init() override {
                if (spi_resources == nullptr) {
                    spi_resources = new TypedArray<HDWISPIResource>();
                }
                spi_resources->append(this);
                uint16_t key = (static_cast<uint16_t>(bus_index) << 8) | chip_select_line;
                bus_cs_to_device_id[key] = spi_resources->size() - 1;
            }

            virtual void clear() override {
                HDWISPIResource::spi_resources->erase(this); // Remove this instance from the static array of SPI resources
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
            
            // Filled by GDScript inside the on_spi_transfer handler; consumed as MISO response.
            PackedByteArray miso_buffer;
            void set_miso_buffer(PackedByteArray p_miso);
            PackedByteArray get_miso_buffer() const;

            // Handlers
            void handle_spi_setup(PackedByteArray request_data);
            void handle_spi_transfer(PackedByteArray request_data);

            // Methods
            // Comm - (Device Registry -> Component's HDWI Resource) -> HDWI
            virtual void dispatch_action(PackedByteArray request_data) override;

            static PackedByteArray get_group_type_representation();
            PackedByteArray get_device_representation() const override;

            // Returns 0-based index in spi_resources for the device identified by id;
            // -1 if not registered.
            static int spi_dev_id_to_device_id(spi_dev_id_t id);
            // GDScript-callable variant.
            static int lookup_spi_device_id(int p_bus_index, int p_chip_select);
    };
}


#endif 