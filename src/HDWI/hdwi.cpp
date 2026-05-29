#include "hdwi.h"

namespace godot {

void HDWIResource::_bind_methods() {
    // Component - (HDWI) -> CommSeq
    ADD_SIGNAL(MethodInfo("on_send", PropertyInfo(Variant::OBJECT, "packet")));
    // Methods
    ClassDB::bind_method(D_METHOD("dispatch_action"), &HDWIResource::dispatch_action);
    // @Export variables bindings
    ClassDB::bind_method(D_METHOD("set_device_name", "device_name"), &HDWIResource::set_device_name);
    ClassDB::bind_method(D_METHOD("get_device_name"), &HDWIResource::get_device_name);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "device_name"), "set_device_name", "get_device_name");
}

void HDWIResource::dispatch_action(PackedByteArray request_data) {
    // Implementation to be added
}


// Setters and getters for device_name
void HDWIResource::set_device_name(const String &p_device_name) {
    device_name = p_device_name;    
}

String HDWIResource::get_device_name() const {
    return device_name;
}

PackedByteArray HDWIResource::get_device_representation() const {
    // New packed byte array to hold the device representation
    PackedByteArray device_representation;

    // Include device name, type and number of lines in the representation
    // For simplicity, we will just convert these to bytes and append them to the array
    // Convert device name to bytes and append
    String device_name = get_device_name();
    char device_name_bytes[32];
    strcpy(device_name_bytes, device_name.utf8().get_data());
    for (int i = 0; i < 32; i++) {
        device_representation.push_back(static_cast<char>(device_name_bytes[i]));
    }

    // Append device type
    device_representation.push_back(static_cast<uint8_t>(get_type()));
    return device_representation;
}


}