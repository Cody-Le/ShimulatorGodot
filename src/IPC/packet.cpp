#include "packet.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;


PacketCPP::PacketCPP() {
    this->valid = false;
}

PacketCPP::~PacketCPP() {
    // No dynamic memory to free, so nothing to do here
}

//Bind all methods and write appropriate signatures for Godot to recognize type + field type correction
void PacketCPP::_bind_methods() {
    ClassDB::bind_method(D_METHOD("generate", "device_index", "cmd_type", "hdwi_type", "time_ns", "bytes"), &PacketCPP::generate);
    ClassDB::bind_method(D_METHOD("generate_from_bytes", "header_bytes"), &PacketCPP::generate_from_bytes);
    ClassDB::bind_method(D_METHOD("get_validity"), &PacketCPP::get_validity);
    ClassDB::bind_method(D_METHOD("get_cmd_id"), &PacketCPP::get_cmd_id);
    ClassDB::bind_method(D_METHOD("get_type"), &PacketCPP::get_type);
    ClassDB::bind_method(D_METHOD("get_device_index"), &PacketCPP::get_device_index);
    ClassDB::bind_method(D_METHOD("get_time_ns"), &PacketCPP::get_time_ns);
    ClassDB::bind_method(D_METHOD("get_data_len"), &PacketCPP::get_data_len);
    ClassDB::bind_method(D_METHOD("_to_string"), &PacketCPP::_to_string);
    ClassDB::bind_method(D_METHOD("set_data", "data"), &PacketCPP::set_data);
    ClassDB::bind_method(D_METHOD("get_data"), &PacketCPP::get_data);
    ClassDB::bind_static_method("PacketCPP", D_METHOD("get_header_len"), &PacketCPP::get_header_len);
    ClassDB::bind_method(D_METHOD("convert_to_bytes"), &PacketCPP::convert_to_bytes);

    // In _bind_methods()
    ClassDB::bind_integer_constant(get_class_static(), "CmdType", "CMD_SYNCH",  (int64_t)CmdType::CMD_SYNCH);
    ClassDB::bind_integer_constant(get_class_static(), "CmdType", "CMD_ACTION", (int64_t)CmdType::CMD_ACTION);

    ClassDB::bind_integer_constant(get_class_static(), "HDWIType", "GPIO", (int64_t)HDWIType::GPIO);
    

}


void PacketCPP::generate(uint8_t device_index, 
                CmdType cmd_type, 
                HDWIType hdwi_type,
                uint64_t time_ns,
                PackedByteArray bytes) {
    this->header = {0};
    this->header.cmd_id = cmd_type;
    this->header.device_index = device_index;
    this->header.type = hdwi_type;
    this->header.time_ns = time_ns;
}  

void PacketCPP::generate_from_bytes(PackedByteArray header_bytes) {
    this->header = {0};
    this->header = *((PacketHeader*)header_bytes.ptrw());
    if(this->header.version != VERSION || unlikely(header_bytes.size() != sizeof(PacketHeader))) {
        this->valid = false;
    } else {
        this->valid = true;
    }
}



bool PacketCPP::get_validity() {
    return this->valid;
}

uint32_t PacketCPP::get_header_length() {
    return sizeof(PacketHeader);
}

uint32_t PacketCPP::get_header_len() {
    return sizeof(PacketHeader);
}

int64_t PacketCPP::get_cmd_id() {
    return (int64_t)header.cmd_id;
}
int64_t PacketCPP::get_type() {
    return (int64_t)header.type;
}
uint8_t PacketCPP::get_device_index() {
    return header.device_index;
}
uint64_t PacketCPP::get_time_ns() {
    return header.time_ns;

}
uint32_t PacketCPP::get_data_len() {
    return header.data_len;

}

// This method is for debugging, print all header field + data as bytes
String PacketCPP::_to_string() const {
    String result = "PacketCPP { ";
    result += "version: " + String::num_int64(header.version) + ", ";
    if(header.cmd_id == CmdType::CMD_SYNCH) {
        result += "cmd_id: CMD_SYNCH, ";
    } else if(header.cmd_id == CmdType::CMD_ACTION) {
        result += "cmd_id: CMD_ACTION, ";
    } else {
        result += "cmd_id: UNKNOWN, ";
    }
    result += "type: " + String::num_int64((uint8_t)header.type) + ", ";
    result += "device_index: " + String::num_int64(header.device_index) + ", ";
    result += "time_ns: " + String::num_int64(header.time_ns) + ", ";
    result += "data_len: " + String::num_int64(header.data_len) + ", ";
    result += "data: " + data.hex_encode();
    result += " }";
    return result;
}

void PacketCPP::set_data(PackedByteArray data) {
    this->data = data;
    this->header.data_len = data.size();
}

PackedByteArray PacketCPP::get_data() const {
    return this->data;
}


PackedByteArray PacketCPP::convert_to_bytes() {
    PackedByteArray bytes;
    bytes.resize(sizeof(PacketHeader) + this->data.size());
    // Copy header
    memcpy(bytes.ptrw(), &this->header, sizeof(PacketHeader));
    // Copy data
    memcpy(bytes.ptrw() + sizeof(PacketHeader), this->data.ptr(), this->data.size());
    return bytes;
}