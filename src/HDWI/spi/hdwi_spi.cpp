#include "./hdwi_spi.h"

namespace godot {

    TypedArray<HDWISPIResource> *HDWISPIResource::spi_resources = nullptr;
    std::unordered_map<uint16_t, int> HDWISPIResource::bus_cs_to_device_id;

    int HDWISPIResource::spi_dev_id_to_device_id(spi_dev_id_t id) {
        uint16_t key = (static_cast<uint16_t>(id.bus_index) << 8) | id.chip_select;
        auto it = bus_cs_to_device_id.find(key);
        if (it == bus_cs_to_device_id.end()) return -1;
        return it->second;
    }

    int HDWISPIResource::lookup_spi_device_id(int p_bus_index, int p_chip_select) {
        uint16_t key = (static_cast<uint16_t>(p_bus_index) << 8) | static_cast<uint8_t>(p_chip_select);
        auto it = bus_cs_to_device_id.find(key);
        if (it == bus_cs_to_device_id.end()) return -1;
        return it->second;
    }

    void HDWISPIResource::_bind_methods() {
        HDWIResource::_bind_methods();

        ClassDB::bind_method(D_METHOD("init"), &HDWISPIResource::init);
        ClassDB::bind_method(D_METHOD("clear"), &HDWISPIResource::clear);
        
        ClassDB::bind_method(D_METHOD("set_max_speed_hz", "max_speed_hz"), &HDWISPIResource::set_max_speed_hz);
        ClassDB::bind_method(D_METHOD("get_max_speed_hz"), &HDWISPIResource::get_max_speed_hz);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "max_speed_hz"), "set_max_speed_hz", "get_max_speed_hz");

        ClassDB::bind_method(D_METHOD("set_mode", "mode"), &HDWISPIResource::set_mode);
        ClassDB::bind_method(D_METHOD("get_mode"), &HDWISPIResource::get_mode);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "mode"), "set_mode", "get_mode");

        ClassDB::bind_method(D_METHOD("set_bits_per_word", "bits_per_word"), &HDWISPIResource::set_bits_per_word);
        ClassDB::bind_method(D_METHOD("get_bits_per_word"), &HDWISPIResource::get_bits_per_word);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "bits_per_word"), "set_bits_per_word", "get_bits_per_word");

        ClassDB::bind_method(D_METHOD("set_chip_select_line", "chip_select_line"), &HDWISPIResource::set_chip_select_line);
        ClassDB::bind_method(D_METHOD("get_chip_select_line"), &HDWISPIResource::get_chip_select_line);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "chip_select_line"), "set_chip_select_line", "get_chip_select_line");

        ClassDB::bind_method(D_METHOD("set_bus_index", "bus_index"), &HDWISPIResource::set_bus_index);
        ClassDB::bind_method(D_METHOD("get_bus_index"), &HDWISPIResource::get_bus_index);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "bus_index"), "set_bus_index", "get_bus_index");

        // Bind methods
        ClassDB::bind_method(D_METHOD("dispatch_action", "request_data"), &HDWISPIResource::dispatch_action);
        ClassDB::bind_static_method("HDWISPIResource", D_METHOD("get_group_type_representation"), &HDWISPIResource::get_group_type_representation);
        ClassDB::bind_static_method("HDWISPIResource", D_METHOD("lookup_spi_device_id", "bus_index", "chip_select"), &HDWISPIResource::lookup_spi_device_id);
        ClassDB::bind_method(D_METHOD("get_device_representation"), &HDWISPIResource::get_device_representation);
        ClassDB::bind_method(D_METHOD("set_miso_buffer", "miso"), &HDWISPIResource::set_miso_buffer);
        ClassDB::bind_method(D_METHOD("get_miso_buffer"), &HDWISPIResource::get_miso_buffer);
        ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "miso_buffer"), "set_miso_buffer", "get_miso_buffer");

        ClassDB::bind_method(D_METHOD("handle_spi_setup", "request_data"), &HDWISPIResource::handle_spi_setup);
        ClassDB::bind_method(D_METHOD("handle_spi_transfer", "request_data"), &HDWISPIResource::handle_spi_transfer);

        ADD_SIGNAL(MethodInfo("on_spi_setup",
            PropertyInfo(Variant::INT, "mode"),
            PropertyInfo(Variant::INT, "bits_per_word"),
            PropertyInfo(Variant::INT, "speed_hz")));
        ADD_SIGNAL(MethodInfo("on_spi_transfer",
            PropertyInfo(Variant::PACKED_BYTE_ARRAY, "tx_data")));
    }

    void HDWISPIResource::set_max_speed_hz(uint32_t p_speed_hz) {
        max_speed_hz = p_speed_hz;
    }

    uint32_t HDWISPIResource::get_max_speed_hz() const {
        return max_speed_hz;
    }

    void HDWISPIResource::set_mode(uint8_t p_mode) {
        mode = p_mode;
    }

    uint8_t HDWISPIResource::get_mode() const {
        return mode;
    }

    void HDWISPIResource::set_bits_per_word(uint8_t p_bits_per_word) {
        bits_per_word = p_bits_per_word;
    }

    uint8_t HDWISPIResource::get_bits_per_word() const {
        return bits_per_word;
    }

    void HDWISPIResource::set_chip_select_line(uint8_t p_chip_select_line) {
        chip_select_line = p_chip_select_line;
    }

    uint8_t HDWISPIResource::get_chip_select_line() const {
        return chip_select_line;
    }

    uint8_t HDWISPIResource::get_bus_index() const {
        return bus_index;
    }


    void HDWISPIResource::set_bus_index(uint8_t p_bus_index) {
        bus_index = p_bus_index;
    }

    PackedByteArray HDWISPIResource::get_group_type_representation() {
        PackedByteArray group_representation;
        group_representation.resize(2);
        group_representation.encode_u8(0, static_cast<uint8_t>(HDWIType::SPI));

        if (spi_resources == nullptr) {
            group_representation.encode_u8(1, 0);
            return group_representation;
        }

        std::unordered_map<uint8_t, std::vector<const HDWISPIResource*>> spi_groups;
        for (const godot::Variant &resource_variant : *HDWISPIResource::spi_resources) {
            const HDWISPIResource *spi_resource = Object::cast_to<HDWISPIResource>(resource_variant);
            spi_groups[spi_resource->get_bus_index()].push_back(spi_resource);
        }

        group_representation.encode_u8(1, static_cast<uint8_t>(spi_groups.size()));

        for (const auto &[bus_idx, resources] : spi_groups) {
            int64_t base = group_representation.size();
            group_representation.resize(base + 2);
            group_representation.encode_u8(base,     bus_idx);
            group_representation.encode_u8(base + 1, static_cast<uint8_t>(resources.size()));
            for (const HDWISPIResource *resource : resources) {
                group_representation.append_array(resource->get_device_representation());
            }
        }

        return group_representation;
    }
    

    PackedByteArray HDWISPIResource::get_device_representation() const {
        PackedByteArray representation = HDWIResource::get_device_representation();
        uint64_t base_size = representation.size();
        // Append SPI-specific properties to the representation
        representation.resize(base_size + sizeof(uint8_t) * 4 + sizeof(uint32_t)); // bus_index, mode, bits_per_word, chip_select_line, speed_hz
        representation.encode_u32(base_size, max_speed_hz);
        representation.encode_u8(base_size + 4, bus_index);
        representation.encode_u8(base_size + 5, mode);
        representation.encode_u8(base_size + 6, bits_per_word);
        representation.encode_u8(base_size + 7, chip_select_line);
        return representation;
    }

    void HDWISPIResource::dispatch_action(PackedByteArray request_data) {
        // Parse the action type from the request data
        if (request_data.size() < 1) {
            // Invalid request, not enough data to determine action type
            return;
        }
        uint8_t action_type = request_data.decode_u8(0);
        PackedByteArray action_data = request_data.slice(1, request_data.size());

        switch (action_type) {
            case SPI_SETUP:
                handle_spi_setup(action_data);
                break;
            case SPI_TRANSFER:
                handle_spi_transfer(action_data);
                break;
            default:
                // Unknown action type
                break;
        }
    }

    void HDWISPIResource::handle_spi_setup(PackedByteArray request_data) {
        // request_data = sim_set_up_request_t (8 bytes):
        //   [0] action (0x01, redundant)
        //   [1] mode (CPOL/CPHA bits)
        //   [2] bits_per_word
        //   [3] chip_select (redundant)
        //   [4..7] speed_hz (u32 LE)
        if (request_data.size() < 8) {
            UtilityFunctions::print("SPI setup packet too short: " + String::num_int64(request_data.size()));
            return;
        }
        mode          = request_data.decode_u8(1);
        bits_per_word = request_data.decode_u8(2);
        max_speed_hz  = request_data.decode_u32(4);
        emit_signal("on_spi_setup", (int)mode, (int)bits_per_word, (int)max_speed_hz);
    }

    void HDWISPIResource::set_miso_buffer(PackedByteArray p_miso) {
        miso_buffer = p_miso;
    }

    PackedByteArray HDWISPIResource::get_miso_buffer() const {
        return miso_buffer;
    }

    void HDWISPIResource::handle_spi_transfer(PackedByteArray request_data) {
        // request_data = N raw MOSI bytes. Must respond with N MISO bytes before the
        // kernel sends the next transfer or the stream desyncs.
        int64_t n = request_data.size();
        miso_buffer.resize(n);  // pre-zero to N bytes; GDScript on_spi_transfer handler may overwrite

        emit_signal("on_spi_transfer", request_data);  // synchronous — handler runs before we continue

        // Clamp or pad in case GDScript set the wrong size.
        if (miso_buffer.size() != n) {
            miso_buffer.resize(n);
        }

        PacketCPP *packet = memnew(PacketCPP);
        packet->generate(CmdType::ACTION, HDWIType::SPI, 0, miso_buffer);
        emit_signal("on_send", packet->convert_to_bytes());
        memdelete(packet);
    }
    
}
