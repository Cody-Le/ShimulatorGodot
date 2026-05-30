#include "hdwi_gpio.h"

namespace godot {

void HDWIGPIOResource::_bind_methods() {
    // Call parent class bindings
    HDWIResource::_bind_methods();

    // @Export variables bindings: line values
    ClassDB::bind_method(D_METHOD("set_gpio_values", "gpio_values"), &HDWIGPIOResource::set_gpio_values);
    ClassDB::bind_method(D_METHOD("get_gpio_values"), &HDWIGPIOResource::get_gpio_values);
    ClassDB::bind_method(D_METHOD("get_device_representation"), &HDWIGPIOResource::get_device_representation);
    ClassDB::bind_method(D_METHOD("dispatch_action", "request_data"), &HDWIGPIOResource::dispatch_action);

    ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "gpio_values"), "set_gpio_values", "get_gpio_values");
    // Signals
    ADD_SIGNAL(MethodInfo("on_gpio_line_change", PropertyInfo(Variant::INT, "line_offset"), PropertyInfo(Variant::INT, "new_value")));
   
}

void HDWIGPIOResource::dispatch_action(PackedByteArray request_data) {
    UtilityFunctions::print("Received dispatch_action call with request data size: " + String::num_int64(request_data.size()));
    // Convert request_data byte data into sim_gpio_request struct
    if (request_data.size() < sizeof(sim_gpio_request)) {
        // Handle error: not enough data to form a valid request
        UtilityFunctions::print("Error: Request data size is too small to form a valid sim_gpio_request struct.");
        return;
    }

    UtilityFunctions::print("Dispatching GPIO action with request data size: " + String::num_int64(request_data.size()));
    sim_gpio_request_t request;
    memcpy(&request, request_data.ptr(), sizeof(sim_gpio_request_t));
    if(request.action == GPIO_SET) {
        handle_gpio_set(request);
    } else if(request.action == GPIO_DIR_OUT) {
        handle_gpio_dir_out(request);
    } else if(request.action == GPIO_DIR_IN) {
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
PackedByteArray HDWIGPIOResource::get_device_representation() {
    // New packed byte array to hold the device representation
    PackedByteArray device_representation = HDWIResource::get_device_representation(); // Start with base representation from parent class
    device_representation.push_back(static_cast<char>(type)); // Append device type to representation
    // Append number of lines
    device_representation.push_back(gpio_values.size());
    return device_representation;
}

// GPIO Handler functions
void HDWIGPIOResource::handle_gpio_get(sim_gpio_request_t request) {
    // Read the value at offset then emit on_send signal with sim_gpio_response
    // as PackedByteArray data
    if(request.offset < gpio_values.size()) {
        uint8_t value = gpio_values.get(request.offset);
        // Create sim_gpio_response struct
        sim_gpio_response_t response;
        response.value = value;
        // Emit signal with response data
        PackedByteArray response_data;
        response_data.resize(sizeof(sim_gpio_response_t));
        memcpy(response_data.ptrw(), &response, sizeof(sim_gpio_response_t));
        UtilityFunctions::print("Prepared response data with value: " + String::num_int64(response.value));
        PacketCPP *packet = memnew(PacketCPP);
        packet->generate(0, CmdType::CMD_ACTION, type, 0, response_data);
        UtilityFunctions::print("Emitting on_send signal with response data size: " + String::num_int64(response_data.size()));
        emit_signal("on_send", packet->convert_to_bytes());
        memdelete(packet);
    } else {
        // Handle error: offset out of range
    }

}

void HDWIGPIOResource::handle_gpio_set(sim_gpio_request_t request) {
    // Set the value at offset to the value in the request, then emit on_send signal to notify of change
    if(request.offset < gpio_values.size()) {
        gpio_values.set(request.offset, request.value);
        // Emit signal for line change
        emit_signal("on_gpio_line_change", request.offset, request.value);
    } else {
        // Handle error: offset out of range
    }
}

void HDWIGPIOResource::handle_gpio_dir_out(sim_gpio_request_t request) {
    // Change gpio_dir at offset to output (1)
    if(request.offset < gpio_dirs.size()) {
        gpio_dirs.set(request.offset, 1);
        handle_gpio_get(request); // Set initial value for output line
    } else {
        // Handle error: offset out of range
    }
}

void HDWIGPIOResource::handle_gpio_dir_in(sim_gpio_request_t request) {
    // Change gpio_dir at offset to input (0)
    if(request.offset < gpio_dirs.size()) {
        gpio_dirs.set(request.offset, 0);
        handle_gpio_get(request); // Clear value for input line
    } else {
        // Handle error: offset out of range
    }
}

}