#include "./hdwi_uart.h"
#include <algorithm>
#include <vector>

namespace godot {

    TypedArray<HDWIUARTResource> *HDWIUARTResource::uart_resources = nullptr;
    std::unordered_map<uint8_t, int> HDWIUARTResource::port_index_to_device_id;

    int HDWIUARTResource::uart_dev_id_to_device_id(uart_dev_id_t id) {
        auto it = port_index_to_device_id.find(id.port_index);
        if (it == port_index_to_device_id.end()) return -1;
        return it->second;
    }

    int HDWIUARTResource::lookup_uart_device_id(int p_port_index) {
        auto it = port_index_to_device_id.find(static_cast<uint8_t>(p_port_index));
        if (it == port_index_to_device_id.end()) return -1;
        return it->second;
    }

    void HDWIUARTResource::_bind_methods() {
        HDWIResource::_bind_methods();

        ClassDB::bind_method(D_METHOD("init"), &HDWIUARTResource::init);
        ClassDB::bind_method(D_METHOD("clear"), &HDWIUARTResource::clear);

        ClassDB::bind_method(D_METHOD("set_port_index", "port_index"), &HDWIUARTResource::set_port_index);
        ClassDB::bind_method(D_METHOD("get_port_index"), &HDWIUARTResource::get_port_index);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "port_index"), "set_port_index", "get_port_index");

        ClassDB::bind_method(D_METHOD("set_from_fsw", "from_fsw"), &HDWIUARTResource::set_from_fsw);
        ClassDB::bind_method(D_METHOD("get_from_fsw"), &HDWIUARTResource::get_from_fsw);
        ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "from_fsw"), "set_from_fsw", "get_from_fsw");

        ClassDB::bind_method(D_METHOD("set_to_fsw", "to_fsw"), &HDWIUARTResource::set_to_fsw);
        ClassDB::bind_method(D_METHOD("get_to_fsw"), &HDWIUARTResource::get_to_fsw);
        ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "to_fsw"), "set_to_fsw", "get_to_fsw");
        ClassDB::bind_method(D_METHOD("queue_to_fsw", "bytes"), &HDWIUARTResource::queue_to_fsw);

        ClassDB::bind_method(D_METHOD("set_rx_irq_enabled", "enabled"), &HDWIUARTResource::set_rx_irq_enabled);
        ClassDB::bind_method(D_METHOD("get_rx_irq_enabled"), &HDWIUARTResource::get_rx_irq_enabled);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "rx_irq_enabled"), "set_rx_irq_enabled", "get_rx_irq_enabled");

        ClassDB::bind_method(D_METHOD("send_rx_data", "bytes"), &HDWIUARTResource::send_rx_data);
        ClassDB::bind_method(D_METHOD("send_break"), &HDWIUARTResource::send_break);
        ClassDB::bind_method(D_METHOD("send_framing_error"), &HDWIUARTResource::send_framing_error);
        ClassDB::bind_method(D_METHOD("send_parity_error"), &HDWIUARTResource::send_parity_error);
        ClassDB::bind_method(D_METHOD("send_overrun"), &HDWIUARTResource::send_overrun);
        ClassDB::bind_method(D_METHOD("send_tx_empty"), &HDWIUARTResource::send_tx_empty);

        ClassDB::bind_method(D_METHOD("dispatch_action", "request_data", "pid"), &HDWIUARTResource::dispatch_action);
        ClassDB::bind_method(D_METHOD("handle_uart_write", "payload"), &HDWIUARTResource::handle_uart_write);
        ClassDB::bind_method(D_METHOD("handle_uart_read", "pid"), &HDWIUARTResource::handle_uart_read);

        ClassDB::bind_method(D_METHOD("get_device_representation"), &HDWIUARTResource::get_device_representation);
        ClassDB::bind_static_method("HDWIUARTResource", D_METHOD("get_group_type_representation"), &HDWIUARTResource::get_group_type_representation);
        ClassDB::bind_static_method("HDWIUARTResource", D_METHOD("lookup_uart_device_id", "port_index"), &HDWIUARTResource::lookup_uart_device_id);

        BIND_CONSTANT(UART_READ);
        BIND_CONSTANT(UART_WRITE);
        BIND_CONSTANT(UART_READ_MAX_CHUNK);

        ADD_SIGNAL(MethodInfo("on_uart_write",
            PropertyInfo(Variant::INT, "port_index"),
            PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
        ADD_SIGNAL(MethodInfo("on_uart_read",
            PropertyInfo(Variant::INT, "port_index")));
    }

    void HDWIUARTResource::set_port_index(int p_port_index) {
        port_index = static_cast<uint8_t>(p_port_index);
    }

    int HDWIUARTResource::get_port_index() const {
        return static_cast<int>(port_index);
    }

    void HDWIUARTResource::set_from_fsw(PackedByteArray p_buffer) {
        from_fsw = p_buffer;
    }

    PackedByteArray HDWIUARTResource::get_from_fsw() const {
        return from_fsw;
    }

    void HDWIUARTResource::set_to_fsw(PackedByteArray p_buffer) {
        to_fsw = p_buffer;
    }

    PackedByteArray HDWIUARTResource::get_to_fsw() const {
        return to_fsw;
    }

    void HDWIUARTResource::queue_to_fsw(PackedByteArray p_bytes) {
        if (rx_irq_enabled) {
            // Push delivery: hand the bytes straight to the FSW as an RX interrupt
            // instead of parking them for the next UART_READ poll.
            send_rx_data(p_bytes);
            return;
        }
        to_fsw.append_array(p_bytes);
    }

    void HDWIUARTResource::set_rx_irq_enabled(bool p_enabled) {
        rx_irq_enabled = p_enabled;
    }

    bool HDWIUARTResource::get_rx_irq_enabled() const {
        return rx_irq_enabled;
    }

    // Build a 1-byte UART IRQ header (used for every sub-type except RX_DATA, which
    // appends its bytes after this header).
    static PackedByteArray uart_irq_header(uint8_t irq) {
        PackedByteArray payload;
        payload.resize(sizeof(sim_uart_irq_payload_t));  // 1
        payload.encode_u8(0, irq);
        return payload;
    }

    void HDWIUARTResource::send_rx_data(PackedByteArray p_bytes) {
        if (p_bytes.is_empty()) {
            return;  // RX_DATA requires N >= 1; nothing to deliver
        }
        PackedByteArray payload = uart_irq_header(UART_IRQ_RX_DATA);
        payload.append_array(p_bytes);
        emit_irq(HDWIType::UART, port_index, payload);
    }

    void HDWIUARTResource::send_break() {
        emit_irq(HDWIType::UART, port_index, uart_irq_header(UART_IRQ_BREAK));
    }

    void HDWIUARTResource::send_framing_error() {
        emit_irq(HDWIType::UART, port_index, uart_irq_header(UART_IRQ_FRAMING_ERR));
    }

    void HDWIUARTResource::send_parity_error() {
        emit_irq(HDWIType::UART, port_index, uart_irq_header(UART_IRQ_PARITY_ERR));
    }

    void HDWIUARTResource::send_overrun() {
        emit_irq(HDWIType::UART, port_index, uart_irq_header(UART_IRQ_OVERRUN));
    }

    void HDWIUARTResource::send_tx_empty() {
        emit_irq(HDWIType::UART, port_index, uart_irq_header(UART_IRQ_TX_EMPTY));
    }

    // Group body for UART (D050): [type][count] followed by count × 32-byte
    // null-padded port names. The kernel assigns port_index sequentially as it walks
    // these entries, so we emit them in port_index order to keep position and
    // port_index in sync. UART carries no extra per-port fields (no representation
    // struct) — line config comes from the FSW via termios, not the wire.
    PackedByteArray HDWIUARTResource::get_group_type_representation() {
        PackedByteArray group_representation;
        group_representation.resize(2);
        group_representation.encode_u8(0, static_cast<uint8_t>(HDWIType::UART));

        if (uart_resources == nullptr) {
            group_representation.encode_u8(1, 0);
            return group_representation;
        }

        std::vector<const HDWIUARTResource*> sorted;
        for (const godot::Variant &resource_variant : *HDWIUARTResource::uart_resources) {
            sorted.push_back(Object::cast_to<HDWIUARTResource>(resource_variant));
        }
        std::sort(sorted.begin(), sorted.end(),
            [](const HDWIUARTResource *a, const HDWIUARTResource *b) {
                return a->port_index < b->port_index;
            });

        group_representation.encode_u8(1, static_cast<uint8_t>(sorted.size()));
        for (const HDWIUARTResource *resource : sorted) {
            group_representation.append_array(resource->get_device_representation());
        }

        return group_representation;
    }

    // UART entries carry only the 32-byte name — no extra per-port fields.
    PackedByteArray HDWIUARTResource::get_device_representation() const {
        return HDWIResource::get_device_representation();
    }

    void HDWIUARTResource::dispatch_action(PackedByteArray request_data, uint32_t pid) {
        // request_data = [0] action, [1..] payload. The dev_id (port_index) was already
        // stripped and used for routing by the registry, which prepends the action byte.
        if (request_data.size() < 1) {
            return;
        }
        uint8_t action = request_data.decode_u8(0);
        PackedByteArray payload = request_data.slice(1, request_data.size());

        switch (action) {
            case UART_WRITE:
                handle_uart_write(payload);   // fire-and-forget, no reply
                break;
            case UART_READ:
                handle_uart_read(pid);
                break;
            default:
                break;
        }
    }

    void HDWIUARTResource::handle_uart_write(PackedByteArray payload) {
        // Pure sink: accumulate the bytes the FSW sent and notify GDScript. NEVER
        // reply — a response on a WRITE desyncs the kernel's read framing.
        from_fsw.append_array(payload);
        emit_signal("on_uart_write", (int)port_index, payload);
    }

    void HDWIUARTResource::handle_uart_read(uint32_t pid) {
        // Last chance for GDScript to fill to_fsw synchronously before we drain it.
        emit_signal("on_uart_read", (int)port_index);

        int64_t n = to_fsw.size() < UART_READ_MAX_CHUNK ? to_fsw.size() : UART_READ_MAX_CHUNK;
        PackedByteArray chunk = to_fsw.slice(0, n);
        to_fsw = to_fsw.slice(n);  // keep whatever didn't fit for the next poll

        // ALWAYS reply, even with zero bytes. Response carries no dev_id — the kernel
        // knows which port it polled.
        PacketCPP *packet = memnew(PacketCPP);
        packet->generate(CmdType::ACTION, HDWIType::UART, sim_time_ns, chunk, pid);
        emit_signal("on_send", packet->convert_to_bytes());
        memdelete(packet);
    }

}
