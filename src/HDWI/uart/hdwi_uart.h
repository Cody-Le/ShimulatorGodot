#ifndef HDWI_UART_H
#define HDWI_UART_H

#include "../hdwi.h"
#include "../../IPC/sim_packet_type.h"
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <unordered_map>

namespace godot {

    // HDWIUARTResource — one virtual UART port the FSW talks to as /dev/ttySIM<port_index>.
    //
    // The port is a byte pipe with two independent directions; Godot is the device
    // model sitting on the far end of the wire:
    //   from_fsw — bytes the FSW transmitted (UART_WRITE). The simulation drains these.
    //   to_fsw   — bytes queued for the FSW to receive (UART_READ). The simulation fills these.
    //
    // The two actions are deliberately asymmetric, which is what keeps the stream framed:
    //   UART_WRITE — pure sink. Append the payload to from_fsw and emit on_uart_write.
    //                NEVER respond; a reply here desyncs the kernel's read loop.
    //   UART_READ  — request/response, polled ~every 10 ms per port. ALWAYS respond,
    //                even with zero bytes. Drains up to UART_READ_MAX_CHUNK bytes off the
    //                front of to_fsw and ships them; the response carries no dev_id.
    //
    // Line config (baud/parity/stop) is not on the wire — the kernel handles termios
    // locally and never forwards it. Model it engine-side if you need real timing
    // (HASP is a fixed 4800 8N1 link, so a hard-coded model suffices).
    //
    // Interface:
    //   1. set port_index (kernel assigns it by position in the sync body, so it must
    //      match; get_group_type_representation() emits ports in port_index order to
    //      guarantee that) and device_name
    //   2. init() to register the port
    //   3. connect on_uart_write to consume bytes the FSW sends; produce bytes for the
    //      FSW by appending to to_fsw (queue_to_fsw()) — the next UART_READ delivers them
    //   4. connect on_send (base) to ship UART_READ responses to the socket
    //
    // Full reference: doc_classes/HDWIUARTResource.xml
    class HDWIUARTResource : public HDWIResource {
        GDCLASS(HDWIUARTResource, HDWIResource)
        private:
            HDWIType type = HDWIType::UART;

        protected:
            static void _bind_methods();
            static TypedArray<HDWIUARTResource> *uart_resources;
            // Key: port_index → 0-based index in uart_resources.
            static std::unordered_map<uint8_t, int> port_index_to_device_id;

        public:
            // Largest chunk a single UART_READ reply will drain off to_fsw. Bounds one
            // poll; the remainder stays queued for the next 10 ms poll.
            static constexpr int64_t UART_READ_MAX_CHUNK = 4096;

            HDWIUARTResource() = default;
            ~HDWIUARTResource() = default;

            // Natural identifier on the wire (inner UART header port_index) and the
            // /dev/ttySIM<port_index> minor. Must match the position the kernel assigns
            // during sync; registration is emitted in this order to guarantee that.
            uint8_t port_index = 0;
            void set_port_index(int p_port_index);
            int  get_port_index() const;

            // Bytes the FSW has written to this port (UART_WRITE), accumulated in order.
            // The simulation consumes these; clear by assigning an empty array.
            PackedByteArray from_fsw;
            void            set_from_fsw(PackedByteArray p_buffer);
            PackedByteArray get_from_fsw() const;

            // Bytes queued for the FSW to read (UART_READ). The simulation fills these;
            // each read drains up to UART_READ_MAX_CHUNK off the front.
            PackedByteArray to_fsw;
            void            set_to_fsw(PackedByteArray p_buffer);
            PackedByteArray get_to_fsw() const;
            // Convenience: append bytes for the FSW to receive on its next read.
            // When rx_irq_enabled is true this instead PUSHES the bytes as a
            // UART_IRQ_RX_DATA interrupt (kernel drops them straight into the TTY flip
            // buffer) and does NOT queue them for the poll path — pick one delivery
            // model per port to avoid the FSW seeing every byte twice.
            void            queue_to_fsw(PackedByteArray p_bytes);

            // Opt-in: route queue_to_fsw through a UART_IRQ_RX_DATA push instead of the
            // polled to_fsw drain. Default false keeps the request/response read model.
            bool rx_irq_enabled = false;
            void set_rx_irq_enabled(bool p_enabled);
            bool get_rx_irq_enabled() const;

            // Explicit RX-side interrupts. send_rx_data pushes received bytes
            // (no-op on an empty array — RX_DATA requires N >= 1); the rest are
            // zero-data error/flow signals inserted into the TTY:
            //   send_break / send_framing_error / send_parity_error / send_overrun
            //   send_tx_empty — uart_write_wakeup(), tells the FSW's write() to refill
            void send_rx_data(PackedByteArray p_bytes);
            void send_break();
            void send_framing_error();
            void send_parity_error();
            void send_overrun();
            void send_tx_empty();

            virtual void init() override {
                if (uart_resources == nullptr) {
                    uart_resources = new TypedArray<HDWIUARTResource>();
                }
                uart_resources->append(this);
                port_index_to_device_id[port_index] = uart_resources->size() - 1;
            }

            virtual void clear() override {
                uart_resources->erase(this);
            }

            void on_send();

            virtual void dispatch_action(PackedByteArray request_data) override;

            void handle_uart_write(PackedByteArray payload);
            void handle_uart_read();

            static PackedByteArray get_group_type_representation();
            PackedByteArray get_device_representation() const override;

            // Returns 0-based index in uart_resources for the port identified by id;
            // -1 if not registered.
            static int uart_dev_id_to_device_id(uart_dev_id_t id);
            // GDScript-callable variant.
            static int lookup_uart_device_id(int p_port_index);
    };
}

#endif
