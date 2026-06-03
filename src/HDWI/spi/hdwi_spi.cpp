#include "./hdwi_spi.h"

namespace godot {

    TypedArray<HDWISPIResource> *HDWISPIResource::spi_resources = nullptr; // Static member variable definition

    void HDWISPIResource::_bind_methods() {
        // Bind signals
        HDWIResource::_bind_methods(); // Bind base class signals

        // Bind properties
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
        ClassDB::bind_method(D_METHOD("get_device_representation"), &HDWISPIResource::get_device_representation);
        ClassDB::bind_method(D_METHOD("handle_spi_setup", "request_data"), &HDWISPIResource::handle_spi_setup);
        ClassDB::bind_method(D_METHOD("handle_spi_transfer", "request_data"), &HDWISPIResource::handle_spi_transfer);
        
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

    //from encoded group len to packet size is: 
    //group_len * (bus_index (1) + group_size (1) + group_size * (device_name (32) + device_representation (8)))
    PackedByteArray HDWISPIResource::get_group_type_representation() {
        PackedByteArray group_representation;
        //group spi over the same spi controller index to some like: spi controller index | len | device1_representation | device2_representation | ...
        std::unordered_map<uint8_t, std::vector<const HDWISPIResource*>> spi_groups;
        for (const godot::Variant &resource_variant: *HDWISPIResource::spi_resources) {
            const HDWISPIResource *spi_resource = Object::cast_to<HDWISPIResource>(resource_variant);
            spi_groups[spi_resource->get_bus_index()].push_back(spi_resource);
        }
        group_representation.resize(2); // Append group type to representation
        group_representation.encode_u8(0, static_cast<uint8_t>(HDWIType::SPI)); // Assuming 0 represents SPI group type
        group_representation.encode_u8(1, spi_groups.size()); // Append number of groups to representation

        uint8_t bytes_size = group_representation.size(); 
        uint8_t bytes_index = group_representation.size(); // Start appending group data after the initial group type and count
        for (const auto &[bus_index, resources] : spi_groups) {
            // Append bus index and number of devices in this group
            group_representation.resize(bytes_size + sizeof(uint8_t) * 2); // bus_index, device count
            bytes_size += sizeof(uint8_t) * 2;
            group_representation.encode_u8(bytes_index++, bus_index);
            group_representation.encode_u8(bytes_index++, resources.size());
            // Append each device's representation
            for (const HDWISPIResource *resource : resources) {
                PackedByteArray device_representation = resource->get_device_representation();
                group_representation.append_array(device_representation);
                bytes_size += device_representation.size();
                bytes_index += device_representation.size();

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

    //Handler placeholders

    void HDWISPIResource::handle_spi_setup(PackedByteArray request_data) {
        // Parse setup parameters from request_data and configure the SPI device accordingly
        // This is a placeholder implementation and should be replaced with actual setup logic
    }

    void HDWISPIResource::handle_spi_transfer(PackedByteArray request_data) {
        // Parse transfer parameters and data from request_data, perform the SPI transfer, and handle the response
        // This is a placeholder implementation and should be replaced with actual transfer logic
    }
    
}
