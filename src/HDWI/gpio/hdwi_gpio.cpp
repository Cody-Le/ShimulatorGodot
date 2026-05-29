#include "hdwi_gpio.h"

namespace godot {

void HDWIGPIOResource::_bind_methods() {
    // Call parent class bindings
    HDWIResource::_bind_methods();

    // @Export variables bindings: num_lines
    ClassDB::bind_method(D_METHOD("set_num_lines", "num_lines"), &HDWIGPIOResource::set_num_lines);
    ClassDB::bind_method(D_METHOD("get_num_lines"), &HDWIGPIOResource::get_num_lines);

    ClassDB::bind_method(D_METHOD("get_device_representation"), &HDWIGPIOResource::get_device_representation);

    // @Export variables bindings: base
    ClassDB::bind_method(D_METHOD("set_base", "base"), &HDWIGPIOResource::set_base);
    ClassDB::bind_method(D_METHOD("get_base"), &HDWIGPIOResource::get_base);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "num_lines"), "set_num_lines", "get_num_lines");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "base"), "set_base", "get_base");
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

// Setters and getters for base
void HDWIGPIOResource::set_base(uint8_t p_base) {
    base = p_base;        
}

uint8_t HDWIGPIOResource::get_base() const {
    return base;
}

// Get device representation as a packed byte array
PackedByteArray HDWIGPIOResource::get_device_representation() {
    // New packed byte array to hold the device representation
    PackedByteArray device_representation = HDWIResource::get_device_representation(); // Start with base representation from parent class
    // Append number of lines
    device_representation.push_back(num_lines);
    // Append base
    device_representation.push_back(base);
    return device_representation;
}

}