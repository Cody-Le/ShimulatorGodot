#include "hdwi.h"

namespace godot {

void HDWIResource::_bind_methods() {
    // Component - (HDWI) -> CommSeq
    ADD_SIGNAL(MethodInfo("on_send", PropertyInfo(Variant::OBJECT, "packet")));
    // Methods
    ClassDB::bind_method(D_METHOD("dispatch_action"), &HDWIResource::dispatch_action);
}

void HDWIResource::dispatch_action() {
    // Implementation to be added
}

void HDWIResource::on_send() {
    // Signal emission
}

}