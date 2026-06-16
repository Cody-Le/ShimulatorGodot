#include "./hdwi_i2c.h"

namespace godot {

    TypedArray<HDWII2CResource> *HDWII2CResource::i2c_resources = nullptr;
    std::unordered_map<uint16_t, int> HDWII2CResource::bus_addr_to_device_id;

    int HDWII2CResource::i2c_dev_id_to_device_id(i2c_dev_id_t id) {
        uint16_t key = (static_cast<uint16_t>(id.bus_index) << 8) | id.address;
        auto it = bus_addr_to_device_id.find(key);
        if (it == bus_addr_to_device_id.end()) return -1;
        return it->second;
    }

    int HDWII2CResource::lookup_i2c_device_id(int p_bus_index, int p_address) {
        uint16_t key = (static_cast<uint16_t>(p_bus_index) << 8) | static_cast<uint8_t>(p_address);
        auto it = bus_addr_to_device_id.find(key);
        if (it == bus_addr_to_device_id.end()) return -1;
        return it->second;
    }

    void HDWII2CResource::_bind_methods() {
        HDWIResource::_bind_methods();

        ClassDB::bind_method(D_METHOD("init"), &HDWII2CResource::init);
        ClassDB::bind_method(D_METHOD("clear"), &HDWII2CResource::clear);

        ClassDB::bind_method(D_METHOD("set_bus_index", "bus_index"), &HDWII2CResource::set_bus_index);
        ClassDB::bind_method(D_METHOD("get_bus_index"), &HDWII2CResource::get_bus_index);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "bus_index"), "set_bus_index", "get_bus_index");

        ClassDB::bind_method(D_METHOD("set_address", "address"), &HDWII2CResource::set_address);
        ClassDB::bind_method(D_METHOD("get_address"), &HDWII2CResource::get_address);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "address"), "set_address", "get_address");

        ClassDB::bind_method(D_METHOD("set_from_master", "from_master"), &HDWII2CResource::set_from_master);
        ClassDB::bind_method(D_METHOD("get_from_master"), &HDWII2CResource::get_from_master);
        ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "from_master"), "set_from_master", "get_from_master");

        ClassDB::bind_method(D_METHOD("set_read_buffer", "read_buffer"), &HDWII2CResource::set_read_buffer);
        ClassDB::bind_method(D_METHOD("get_read_buffer"), &HDWII2CResource::get_read_buffer);
        ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "read_buffer"), "set_read_buffer", "get_read_buffer");

        ClassDB::bind_method(D_METHOD("send_smbus_alert"), &HDWII2CResource::send_smbus_alert);
        ClassDB::bind_method(D_METHOD("send_smbus_alert_addr", "alerting_addr"), &HDWII2CResource::send_smbus_alert_addr);

        ClassDB::bind_method(D_METHOD("dispatch_action", "request_data", "pid"), &HDWII2CResource::dispatch_action);
        ClassDB::bind_method(D_METHOD("handle_i2c_write", "payload"), &HDWII2CResource::handle_i2c_write);
        ClassDB::bind_method(D_METHOD("handle_i2c_read", "payload", "pid"), &HDWII2CResource::handle_i2c_read);

        ClassDB::bind_method(D_METHOD("get_device_representation"), &HDWII2CResource::get_device_representation);
        ClassDB::bind_static_method("HDWII2CResource", D_METHOD("get_group_type_representation"), &HDWII2CResource::get_group_type_representation);
        ClassDB::bind_static_method("HDWII2CResource", D_METHOD("lookup_i2c_device_id", "bus_index", "address"), &HDWII2CResource::lookup_i2c_device_id);

        BIND_CONSTANT(I2C_WRITE);
        BIND_CONSTANT(I2C_READ);

        ADD_SIGNAL(MethodInfo("on_i2c_write",
            PropertyInfo(Variant::INT, "address"),
            PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
        ADD_SIGNAL(MethodInfo("on_i2c_read",
            PropertyInfo(Variant::INT, "address"),
            PropertyInfo(Variant::INT, "length")));
    }

    void HDWII2CResource::set_bus_index(int p_bus_index) {
        bus_index = static_cast<uint8_t>(p_bus_index);
    }

    int HDWII2CResource::get_bus_index() const {
        return static_cast<int>(bus_index);
    }

    void HDWII2CResource::set_address(int p_address) {
        address = static_cast<uint8_t>(p_address);
    }

    int HDWII2CResource::get_address() const {
        return static_cast<int>(address);
    }

    void HDWII2CResource::set_from_master(PackedByteArray p_buffer) {
        from_master = p_buffer;
    }

    PackedByteArray HDWII2CResource::get_from_master() const {
        return from_master;
    }

    void HDWII2CResource::set_read_buffer(PackedByteArray p_buffer) {
        read_buffer = p_buffer;
    }

    PackedByteArray HDWII2CResource::get_read_buffer() const {
        return read_buffer;
    }

    // Group body for I2C (D050): [type][num_bus] followed by one entry per bus, in
    // ascending bus_index order so kernel-assigned adapter indices line up with
    // /dev/i2c-N. Each bus entry is [bus_index][num_dev] then num_dev device
    // representations (32-byte name + 1-byte address). One bus costs
    // 2 + num_dev*33 bytes.
    PackedByteArray HDWII2CResource::get_group_type_representation() {
        PackedByteArray group_representation;
        group_representation.resize(2);
        group_representation.encode_u8(0, static_cast<uint8_t>(HDWIType::I2C));

        if (i2c_resources == nullptr) {
            group_representation.encode_u8(1, 0);
            return group_representation;
        }

        // std::map keeps buses ordered by bus_index; the kernel assigns adapter
        // numbers positionally as it walks the manifest, so order must be ascending.
        std::map<uint8_t, std::vector<const HDWII2CResource*>> i2c_groups;
        for (const godot::Variant &resource_variant : *HDWII2CResource::i2c_resources) {
            const HDWII2CResource *i2c_resource = Object::cast_to<HDWII2CResource>(resource_variant);
            i2c_groups[i2c_resource->get_bus_index()].push_back(i2c_resource);
        }

        group_representation.encode_u8(1, static_cast<uint8_t>(i2c_groups.size()));

        for (const auto &[bus_idx, resources] : i2c_groups) {
            int64_t base = group_representation.size();
            group_representation.resize(base + 2);
            group_representation.encode_u8(base,     bus_idx);
            group_representation.encode_u8(base + 1, static_cast<uint8_t>(resources.size()));
            for (const HDWII2CResource *resource : resources) {
                group_representation.append_array(resource->get_device_representation());
            }
        }

        return group_representation;
    }

    // I2C entries carry the 32-byte name followed by the 1-byte 7-bit address.
    PackedByteArray HDWII2CResource::get_device_representation() const {
        PackedByteArray representation = HDWIResource::get_device_representation();
        uint64_t base_size = representation.size();
        representation.resize(base_size + sizeof(uint8_t));
        representation.encode_u8(base_size, address);
        return representation;
    }

    void HDWII2CResource::send_smbus_alert() {
        send_smbus_alert_addr(address);
    }

    void HDWII2CResource::send_smbus_alert_addr(int alerting_addr) {
        PackedByteArray payload;
        payload.resize(sizeof(sim_i2c_irq_payload_t));  // 2
        payload.encode_u8(0, I2C_IRQ_SMBUS_ALERT);
        payload.encode_u8(1, static_cast<uint8_t>(alerting_addr));
        emit_irq(HDWIType::I2C, bus_index, payload);
    }

    void HDWII2CResource::dispatch_action(PackedByteArray request_data, uint32_t pid) {
        // request_data = [0] action, [1..] payload. The dev_id (bus_index, address)
        // was already stripped and used for routing by the registry, which prepends
        // the action byte taken from the dev_id.
        if (request_data.size() < 1) {
            return;
        }
        uint8_t action = request_data.decode_u8(0);
        PackedByteArray payload = request_data.slice(1, request_data.size());

        switch (action) {
            case I2C_WRITE:
                handle_i2c_write(payload);   // fire-and-forget, no reply
                break;
            case I2C_READ:
                handle_i2c_read(payload, pid);
                break;
            default:
                break;
        }
    }

    void HDWII2CResource::handle_i2c_write(PackedByteArray payload) {
        // Pure sink: accumulate the bytes the FSW wrote and notify GDScript. NEVER
        // reply — a response on a WRITE desyncs the kernel's read framing. A
        // zero-length payload is an address probe / SMBus quick-command.
        from_master.append_array(payload);
        emit_signal("on_i2c_write", (int)address, payload);
    }

    void HDWII2CResource::handle_i2c_read(PackedByteArray payload, uint32_t pid) {
        // payload = u16 LE requested byte count. The kernel hard-checks that the
        // reply's data_len equals this, so we must respond with EXACTLY len bytes.
        int64_t len = payload.size() >= 2 ? payload.decode_u16(0) : 0;
        read_buffer.resize(len);  // pre-zero to len; on_i2c_read handler may overwrite

        emit_signal("on_i2c_read", (int)address, (int)len);  // synchronous — handler fills read_buffer

        // Clamp or pad in case GDScript set the wrong size.
        if (read_buffer.size() != len) {
            read_buffer.resize(len);
        }

        // Response carries no dev_id — the socket is strictly in-order, so the kernel
        // knows which read it answers.
        PacketCPP *packet = memnew(PacketCPP);
        packet->generate(CmdType::ACTION, HDWIType::I2C, sim_time_ns, read_buffer, pid);
        emit_signal("on_send", packet->convert_to_bytes());
        memdelete(packet);
    }

}
