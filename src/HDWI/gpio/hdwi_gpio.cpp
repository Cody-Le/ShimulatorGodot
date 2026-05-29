#include "hdwi_gpio.h"

namespace godot {

void HDWIGPIOResource::_bind_methods() {
    // Call parent class bindings
    HDWIResource::_bind_methods();

    // @Export variables bindings: num_lines
    ClassDB::bind_method(D_METHOD("set_num_lines", "num_lines"), &HDWIGPIOResource::set_num_lines);
    ClassDB::bind_method(D_METHOD("get_num_lines"), &HDWIGPIOResource::get_num_lines);

    ClassDB::bind_method(D_METHOD("get_device_representation"), &HDWIGPIOResource::get_device_representation);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "num_lines"), "set_num_lines", "get_num_lines");
}

void HDWIGPIOResource::dispatch_action(PackedByteArray request_data) {
    // Implementation to be added
}


// Setters and getters for num_lines
void HDWIGPIOResource::set_num_lines(uint8_t p_num_lines) {
    num_lines = p_num_lines;        
}

uint8_t HDWIGPIOResource::get_num_lines() const {
    return num_lines;
}

// Get device representation as a packed byte array
PackedByteArray HDWIGPIOResource::get_device_representation() {
    // New packed byte array to hold the device representation
    PackedByteArray device_representation = HDWIResource::get_device_representation(); // Start with base representation from parent class
    // Append number of lines
    device_representation.push_back(num_lines);
    return device_representation;
}

}