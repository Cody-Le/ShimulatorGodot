#include "hdwi_gpio.h"

namespace godot {

std::unordered_map<uint8_t, int> HDWIGPIOResource::chip_index_to_device_id;

void HDWIGPIOResource::_bind_methods() {
    HDWIResource::_bind_methods();

    ClassDB::bind_method(D_METHOD("set_gpio_values", "gpio_values"), &HDWIGPIOResource::set_gpio_values);
    ClassDB::bind_method(D_METHOD("get_gpio_values"), &HDWIGPIOResource::get_gpio_values);
    ClassDB::bind_method(D_METHOD("get_device_representation"), &HDWIGPIOResource::get_device_representation);
    ClassDB::bind_method(D_METHOD("dispatch_action", "request_data", "pid"), &HDWIGPIOResource::dispatch_action);
    ClassDB::bind_method(D_METHOD("init"), &HDWIGPIOResource::init);
    ClassDB::bind_method(D_METHOD("clear"), &HDWIGPIOResource::clear);
    ClassDB::bind_method(D_METHOD("set_chip_index", "chip_index"), &HDWIGPIOResource::set_chip_index);
    ClassDB::bind_method(D_METHOD("get_chip_index"), &HDWIGPIOResource::get_chip_index);
    ClassDB::bind_static_method("HDWIGPIOResource", D_METHOD("get_group_type_representation"), &HDWIGPIOResource::get_group_type_representation);
    ClassDB::bind_static_method("HDWIGPIOResource", D_METHOD("lookup_gpio_device_id", "chip_index"), &HDWIGPIOResource::lookup_gpio_device_id);

    ClassDB::bind_method(D_METHOD("set_irq_enabled", "enabled"), &HDWIGPIOResource::set_irq_enabled);
    ClassDB::bind_method(D_METHOD("get_irq_enabled"), &HDWIGPIOResource::get_irq_enabled);
    ClassDB::bind_method(D_METHOD("send_line_change_irq", "line", "value"), &HDWIGPIOResource::send_line_change_irq);

    ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "gpio_values"), "set_gpio_values", "get_gpio_values");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "chip_index"), "set_chip_index", "get_chip_index");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "irq_enabled"), "set_irq_enabled", "get_irq_enabled");
    ADD_SIGNAL(MethodInfo("on_gpio_line_change", PropertyInfo(Variant::INT, "line_offset"), PropertyInfo(Variant::INT, "new_value")));
}

void HDWIGPIOResource::set_irq_enabled(bool p_enabled) {
    irq_enabled = p_enabled;
}

bool HDWIGPIOResource::get_irq_enabled() const {
    return irq_enabled;
}

void HDWIGPIOResource::send_line_change_irq(int line, int value) {
    PackedByteArray payload;
    payload.resize(sizeof(sim_gpio_irq_payload_t));  // 3
    payload.encode_u8(0, GPIO_IRQ_LINE_CHANGE);
    payload.encode_u8(1, static_cast<uint8_t>(line));
    payload.encode_u8(2, static_cast<uint8_t>(value));
    emit_irq(HDWIType::GPIO, chip_index, payload);
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

void HDWIGPIOResource::dispatch_action(PackedByteArray request_data, uint32_t pid) {
    //UtilityFunctions::print("Received dispatch_action call with request data size: " + String::num_int64(request_data.size()));
    if (request_data.size() < (int64_t)sizeof(GpioRequest)) {
        UtilityFunctions::print("Error: Request data size is too small to form a valid GpioRequest.");
        return;
    }

    GpioRequest request;
    memcpy(&request, request_data.ptr(), sizeof(GpioRequest));
    if (request.action == GPIO_SET) {
        handle_gpio_set(request);   // fire-and-forget, no reply, pid not needed
    } else if (request.action == GPIO_DIR_OUT) {
        handle_gpio_dir_out(request, pid);
    } else if (request.action == GPIO_DIR_IN) {
        handle_gpio_dir_in(request, pid);
    } else {
        handle_gpio_get(request, pid);
    }
}

void HDWIGPIOResource::set_gpio_values(PackedInt32Array p_gpio_values) {
    //Find the difference between current gpio_values and new p_gpio_values to determine which lines have changed
    for (int i = 0; i < p_gpio_values.size(); i++) {
        if (i >= gpio_values.size() || gpio_values.get(i) != p_gpio_values.get(i)) {
            // Emit signal for line change
            emit_signal("on_gpio_line_change", i, p_gpio_values.get(i));
            // Engine-driven edge on an INPUT line is a hardware interrupt: push it to
            // the FSW. A line the FSW configured as OUTPUT (gpio_dirs[i] == 1) is
            // driven by the FSW, not us, so it never raises an IRQ. Unconfigured
            // lines default to input.
            bool is_input = (i >= gpio_dirs.size()) || gpio_dirs.get(i) == 0;
            if (irq_enabled && is_input) {
                send_line_change_irq(i, p_gpio_values.get(i));
            }
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
void HDWIGPIOResource::handle_gpio_get(GpioRequest request, uint32_t pid) {
    if (request.offset < gpio_values.size()) {
        GpioResponse response;
        response.status = 0;
        response.value  = static_cast<uint8_t>(gpio_values.get(request.offset));
        PackedByteArray response_data;
        response_data.resize(sizeof(GpioResponse));
        memcpy(response_data.ptrw(), &response, sizeof(GpioResponse));
        PacketCPP *packet = memnew(PacketCPP);
        packet->generate(CmdType::ACTION, type, sim_time_ns, response_data, pid);
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

void HDWIGPIOResource::handle_gpio_dir_out(GpioRequest request, uint32_t pid) {
    if(request.offset < gpio_dirs.size()) {
        gpio_dirs.set(request.offset, 1);
        handle_gpio_get(request, pid);
    } else {
        // Handle error: offset out of range
    }
}

void HDWIGPIOResource::handle_gpio_dir_in(GpioRequest request, uint32_t pid) {
    if(request.offset < gpio_dirs.size()) {
        gpio_dirs.set(request.offset, 0);
        handle_gpio_get(request, pid);
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