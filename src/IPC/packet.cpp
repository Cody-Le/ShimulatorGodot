#include "packet.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;


void PacketCPP::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_validity"), &PacketCPP::get_validity);
    ClassDB::bind_method(D_METHOD("get_header_length"), &PacketCPP::get_header_length);
    
}
bool PacketCPP::get_validity() {
    return this->valid;
}

uint32_t PacketCPP::get_header_length() {
    return sizeof(PacketHeader);
}

