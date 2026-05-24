#ifndef PACKET_H
#define PACKET_H


#include "../HDWI/hdwi.h"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <cstdint>


namespace godot {

    typedef uint8_t cmd_id_t;
    typedef uint8_t action_id_t;

    #define VERSION 4

    #define CMD_SYNCH ((cmd_id_t)0x01)
    #define CMD_ACTION ((cmd_id_t)0x02)


    struct PacketHeader {
        uint16_t version;
        cmd_id_t cmd_id;
        hdwi_type_t type;
        uint8_t device_index;
        char reversed[3];
        uint64_t time_ns;
        uint32_t data_len;
    };





    class PacketCPP : public RefCounted {
        GDCLASS(PacketCPP, RefCounted)
        private:
            bool valid = false;
        
        protected:
            static void _bind_methods();
        
        public:
            static uint32_t get_header_length();
            PacketHeader header;
            PackedByteArray data;
            PacketCPP();
            ~PacketCPP();
            bool get_validity();
    };
}


#endif