#include "hdwi_gpio.h"

namespace godot {

std::unordered_map<uint8_t, int> HDWIGPIOResource::chip_index_to_device_id;

void HDWIGPIOResource::_bind_methods() {
    HDWIResource::_bind_methods();

    ClassDB::bind_method(D_METHOD("set_gpio_values", "gpio_values"), &HDWIGPIOResource::set_gpio_values);
    ClassDB::bind_method(D_METHOD("get_gpio_values"), &HDWIGPIOResource::get_gpio_values);
    ClassDB::bind_method(D_METHOD("get_device_representation"), &HDWIGPIOResource::get_device_representation);
    ClassDB::bind_method(D_METHOD("dispatch_action", "request_data"), &HDWIGPIOResource::dispatch_action);
    ClassDB::bind_method(D_METHOD("init"), &HDWIGPIOResource::init);
    ClassDB::bind_method(D_METHOD("clear"), &HDWIGPIOResource::clear);
    ClassDB::bind_method(D_METHOD("set_chip_index", "chip_index"), &HDWIGPIOResource::set_chip_index);
    ClassDB::bind_method(D_METHOD("get_chip_index"), &HDWIGPIOResource::get_chip_index);
    ClassDB::bind_static_method("HDWIGPIOResource", D_METHOD("get_group_type_representation"), &HDWIGPIOResource::get_group_type_representation);
    ClassDB::bind_static_method("HDWIGPIOResource", D_METHOD("lookup_gpio_device_id", "chip_index"), &HDWIGPIOResource::lookup_gpio_device_id);

    ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "gpio_values"), "set_gpio_values", "get_gpio_values");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "chip_index"), "set_chip_index", "get_chip_index");
    ADD_SIGNAL(MethodInfo("on_gpio_line_change", PropertyInfo(Variant::INT, "line_offset"), PropertyInfo(Variant::INT, "new_value")));
}

void HDWIGPIOResource::set_chip_index(int p_chip_index) {
    chip_index = static_cast<uint8_t>(p_chip_index);
}

int HDWIGPIOResource::get_chip_index() const {
    return static_cast<int>(chip_index);
}

int HDWIGPIOResource::gpio_dev_id_to_device_id(gpio_dev_id_t id) {
    auto it = chip_index_to_device_id.find(id.chip_index);
    if (it == chip_index_to_device_id.end()) return -1;
    return it->second;
}

int HDWIGPIOResource::lookup_gpio_device_id(int p_chip_index) {
    auto it = chip_index_to_device_id.find(static_cast<uint8_t>(p_chip_index));
    if (it == chip_index_to_device_id.end()) return -1;
    return it->second;
}

void HDWIGPIOResource::dispatch_action(PackedByteArray request_data) {
    UtilityFunctions::print("Received dispatch_action call with request data size: " + String::num_int64(request_data.size()));
    if (request_data.size() < (int64_t)sizeof(GpioRequest)) {
        UtilityFunctions::print("Error: Request data size is too small to form a valid GpioRequest.");
        return;
    }

    GpioRequest request;
    memcpy(&request, request_data.ptr(), sizeof(GpioRequest));
    if (request.action == GPIO_SET) {
        handle_gpio_set(request);
    } else if (request.action == GPIO_DIR_OUT) {
        handle_gpio_dir_out(request);
    } else if (request.action == GPIO_DIR_IN) {
        handle_gpio_dir_in(request);
    } else {
        handle_gpio_get(request);
    }
}

void HDWIGPIOResource::set_gpio_values(PackedInt32Array p_gpio_values) {
    //Find the difference between current gpio_values and new p_gpio_values to determine which lines have changed
    for (int i = 0; i < p_gpio_values.size(); i++) {
        if (i >= gpio_values.size() || gpio_values.get(i) != p_gpio_values.get(i)) {
            // Emit signal for line change
            emit_signal("on_gpio_line_change", i, p_gpio_values.get(i));
        }
    }
    //Shrink gpio_dir with the size of gpio_values
    gpio_dirs.resize(p_gpio_values.size());
    gpio_values = p_gpio_values;
}

PackedInt32Array HDWIGPIOResource::get_gpio_values() const {
    return gpio_values;
}



// Get device representation as a packed byte array
PackedByteArray HDWIGPIOResource::get_device_representation() const{
    // New packed byte array to hold the device representation
    PackedByteArray device_representation = HDWIResource::get_device_representation(); // Start with base representation from parent class
    device_representation.push_back(static_cast<char>(type)); // Append device type to representation
    // Append number of lines
    device_representation.push_back(gpio_values.size());
    return device_representation;
}

// GPIO Handler functions
void HDWIGPIOResource::handle_gpio_get(GpioRequest request) {
    if (request.offset < gpio_values.size()) {
        GpioResponse response;
        response.status = 0;
        response.value  = static_cast<uint8_t>(gpio_values.get(request.offset));
        PackedByteArray response_data;
        response_data.resize(sizeof(GpioResponse));
        memcpy(response_data.ptrw(), &response, sizeof(GpioResponse));
        UtilityFunctions::print("Prepared response data with value: " + String::num_int64(response.value));
        PacketCPP *packet = memnew(PacketCPP);
        packet->generate(CmdType::ACTION, type, 0, response_data);
        UtilityFunctions::print("Emitting on_send signal with response data size: " + String::num_int64(response_data.size()));
        emit_signal("on_send", packet->convert_to_bytes());
        memdelete(packet);
    } else {
        // Handle error: offset out of range
    }

}

void HDWIGPIOResource::handle_gpio_set(GpioRequest request) {
    // Set the value at offset to the value in the request, then emit on_send signal to notify of change
    if(request.offset < gpio_values.size()) {
        gpio_values.set(request.offset, request.value);
        // Emit signal for line change
        emit_signal("on_gpio_line_change", request.offset, request.value);
    } else {
        // Handle error: offset out of range
    }
}

void HDWIGPIOResource::handle_gpio_dir_out(GpioRequest request) {
    // Change gpio_dir at offset to output (1)
    if(request.offset < gpio_dirs.size()) {
        gpio_dirs.set(request.offset, 1);
        handle_gpio_get(request); // Set initial value for output line
    } else {
        // Handle error: offset out of range
    }
}

void HDWIGPIOResource::handle_gpio_dir_in(GpioRequest request) {
    // Change gpio_dir at offset to input (0)
    if(request.offset < gpio_dirs.size()) {
        gpio_dirs.set(request.offset, 0);
        handle_gpio_get(request); // Clear value for input line
    } else {
        // Handle error: offset out of range
    }


}

// Static member variable definition
TypedArray<HDWIGPIOResource> *HDWIGPIOResource::gpio_resources = nullptr;
// Encoded len to the representation size is: 
// len * (base_size (32) + type (1) + ngpio (1))
// Get all GPIO resources
PackedByteArray HDWIGPIOResource::get_group_type_representation() {
    PackedByteArray group_representation;
    group_representation.resize(2);
    group_representation.encode_u8(0, static_cast<uint8_t>(HDWIType::GPIO));
    if (gpio_resources == nullptr) {
        group_representation.encode_u8(1, 0);
        return group_representation;
    }
    group_representation.encode_u8(1, gpio_resources->size());
    for (const auto &resource_variant: *HDWIGPIOResource::gpio_resources) {
        const HDWIGPIOResource *gpio_resource = Object::cast_to<HDWIGPIOResource>(resource_variant);
        PackedByteArray device_representation = gpio_resource->get_device_representation();
        group_representation.append_array(device_representation);
        
    }
    return group_representation;

}

}