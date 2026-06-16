#ifndef HDWI_I2C_H
#define HDWI_I2C_H

#include "../hdwi.h"
#include "../../IPC/sim_packet_type.h"
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <unordered_map>
#include <map>
#include <vector>

namespace godot {

    // HDWII2CResource — one I2C slave addressed by (bus_index, address) on
    // /dev/i2c-<bus_index>.
    //
    // The kernel decomposes every FSW transfer — including SMBus calls — into raw
    // WRITE/READ messages before they reach the engine, so this resource only ever
    // sees two asymmetric actions. That asymmetry is what keeps the shared stream
    // framed (mirrors UART):
    //   I2C_WRITE — master -> slave. A pure sink: the payload is appended to
    //               from_master and on_i2c_write is emitted. NEVER replies; a
    //               response on a write desyncs the kernel's read loop. A zero-length
    //               write is an address probe / SMBus quick-command — still no reply.
    //   I2C_READ  — master <- slave. Request/response: the payload is a u16 LE byte
    //               count. ALWAYS replies with EXACTLY that many bytes (the kernel
    //               hard-checks data_len == requested len and returns -EIO on
    //               mismatch). GDScript fills read_buffer synchronously in the
    //               on_i2c_read handler; wrong-length buffers are clamped/padded.
    //
    // The canonical "read register R" arrives as two frames in order — a WRITE
    // carrying [R] (set the register pointer), immediately followed by a READ for the
    // data. There is no repeated-start flag on the wire; just honor frame order per
    // (bus, address).
    //
    // Interface:
    //   1. set bus_index and address (the routing key) and device_name
    //   2. init() to register the device
    //   3. connect on_i2c_write to consume bytes the FSW writes (latch a register
    //      pointer, store config, …)
    //   4. connect on_i2c_read; in the handler fill read_buffer with EXACTLY the
    //      requested length SYNCHRONOUSLY — the handler runs inline before the
    //      response is framed, so deferring it desyncs the stream
    //   5. connect on_send (base) to ship the READ response to the socket
    //
    // Full reference: doc_classes/HDWII2CResource.xml
    class HDWII2CResource : public HDWIResource {
        GDCLASS(HDWII2CResource, HDWIResource)
        private:
            HDWIType type = HDWIType::I2C;

        protected:
            static void _bind_methods();
            static TypedArray<HDWII2CResource> *i2c_resources;
            // Key: (bus_index << 8) | address → 0-based index in i2c_resources.
            static std::unordered_map<uint16_t, int> bus_addr_to_device_id;

        public:
            HDWII2CResource() = default;
            ~HDWII2CResource() = default;

            // Adapter index — the /dev/i2c-N this slave lives on. Part of the routing
            // key; MUST default to a deterministic value, else an unset .tscn property
            // leaves it indeterminate and the transfer is dropped.
            uint8_t bus_index = 0;
            void set_bus_index(int p_bus_index);
            int  get_bus_index() const;

            // 7-bit slave address. Other half of the routing key — same determinism
            // requirement as bus_index.
            uint8_t address = 0;
            void set_address(int p_address);
            int  get_address() const;

            // Bytes the FSW has written to this slave (I2C_WRITE), accumulated in
            // order. The simulation consumes these; clear by assigning an empty array.
            // Each write also arrives live via on_i2c_write.
            PackedByteArray from_master;
            void            set_from_master(PackedByteArray p_buffer);
            PackedByteArray get_from_master() const;

            // Filled by GDScript inside the on_i2c_read handler; consumed as the READ
            // response. Length must equal the requested byte count (clamped/padded).
            PackedByteArray read_buffer;
            void            set_read_buffer(PackedByteArray p_buffer);
            PackedByteArray get_read_buffer() const;

            virtual void init() override {
                if (i2c_resources == nullptr) {
                    i2c_resources = new TypedArray<HDWII2CResource>();
                }
                i2c_resources->append(this);
                uint16_t key = (static_cast<uint16_t>(bus_index) << 8) | address;
                bus_addr_to_device_id[key] = i2c_resources->size() - 1;
            }

            virtual void clear() override {
                HDWII2CResource::i2c_resources->erase(this);
            }

            void on_send();

            // Raise an SMBus #ALERT on this slave's bus (device_id = bus_index). The
            // kernel hands it to a registered ARA client, which then reads address
            // 0x0C — that read comes back as a normal I2C_READ the FSW must answer with
            // the alerting address. send_smbus_alert() reports this slave's own
            // address; pass an explicit one to alert on behalf of another device.
            void send_smbus_alert();
            void send_smbus_alert_addr(int alerting_addr);

            virtual void dispatch_action(PackedByteArray request_data) override;

            void handle_i2c_write(PackedByteArray payload);
            void handle_i2c_read(PackedByteArray payload);

            static PackedByteArray get_group_type_representation();
            PackedByteArray get_device_representation() const override;

            // Returns 0-based index in i2c_resources for the slave identified by id;
            // -1 if not registered.
            static int i2c_dev_id_to_device_id(i2c_dev_id_t id);
            // GDScript-callable variant.
            static int lookup_i2c_device_id(int p_bus_index, int p_address);
    };
}

#endif
