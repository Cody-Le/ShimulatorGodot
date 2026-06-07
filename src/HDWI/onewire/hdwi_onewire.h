#ifndef HDWI_ONEWIRE_H
#define HDWI_ONEWIRE_H

#include "../hdwi.h"
#include "../../IPC/sim_packet_type.h"
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <unordered_map>

namespace godot {

    // HDWIOneWireResource — one 1-Wire sensor the FSW reads through sysfs
    // (e.g. a DS18B20). Reads only; the resource is device-agnostic and ships
    // read_buffer back verbatim.
    //
    // Interface:
    //   1. set sensor_index (must match the kernel's position-assigned index;
    //      get_group_type_representation() emits sensors in sensor_index order to
    //      guarantee this) and device_name (the ROM id string)
    //   2. init() to register the sensor
    //   3. connect on_onewire_read; in the handler branch on the action and fill
    //      read_buffer in the format the opened sysfs file expects:
    //        ONEWIRE_READ_TEMPERATURE -> "temperature" (millidegrees ASCII)
    //        ONEWIRE_READ_SLAVE       -> "w1_slave" (raw bytes + CRC/t= line)
    //        ONEWIRE_READ_RAW         -> "rw" generic raw bytes
    //   4. connect on_send (base) to ship the response to the socket
    //
    // Full reference: doc_classes/HDWIOneWireResource.xml
    class HDWIOneWireResource : public HDWIResource {
        GDCLASS(HDWIOneWireResource, HDWIResource)
        private:
            HDWIType type = HDWIType::ONEWIRE;

        protected:
            static void _bind_methods();
            static TypedArray<HDWIOneWireResource> *onewire_resources;
            // Key: sensor_index → 0-based index in onewire_resources.
            static std::unordered_map<uint8_t, int> sensor_index_to_device_id;

        public:
            HDWIOneWireResource() = default;
            ~HDWIOneWireResource() = default;

            // The kernel assigns sensor_index by position in the CMD_SYNCH group body.
            // Godot must register sensors so that this field matches that position;
            // get_group_type_representation() emits sensors in sensor_index order to
            // guarantee that mapping.
            uint8_t sensor_index = 0;
            void set_sensor_index(int p_sensor_index);
            int  get_sensor_index() const;

            // Raw response payload sent back on a read. GDScript fills this inside the
            // on_onewire_read handler; the resource is device-agnostic and ships the
            // bytes verbatim (a DS18B20 script would encode int32 millidegrees here).
            PackedByteArray read_buffer;
            void            set_read_buffer(PackedByteArray p_buffer);
            PackedByteArray get_read_buffer() const;

            virtual void init() override {
                if (onewire_resources == nullptr) {
                    onewire_resources = new TypedArray<HDWIOneWireResource>();
                }
                onewire_resources->append(this);
                sensor_index_to_device_id[sensor_index] = onewire_resources->size() - 1;
            }

            virtual void clear() override {
                onewire_resources->erase(this);
            }

            void on_send();

            virtual void dispatch_action(PackedByteArray request_data) override;

            void handle_onewire_read(uint8_t action);

            static PackedByteArray get_group_type_representation();
            PackedByteArray get_device_representation() const override;

            // Returns 0-based index in onewire_resources for the sensor identified by
            // id; -1 if not registered.
            static int onewire_dev_id_to_device_id(onewire_dev_id_t id);
            // GDScript-callable variant.
            static int lookup_onewire_device_id(int p_sensor_index);
    };
}

#endif
