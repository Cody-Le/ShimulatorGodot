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


}