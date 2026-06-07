#ifndef PACKET_H
#define PACKET_H


#include "sim_packet_type.h"
#include "../HDWI/hdwi_type.h"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <cstdint>


namespace godot {

    #define VERSION 4

    enum class CmdType : uint8_t {
        SYNCH  = 0x01,
        ACTION = 0x02
    };





    class PacketCPP : public RefCounted {
        GDCLASS(PacketCPP, RefCounted)
        private:
            bool valid = false;
            simcall_header_t header;
            PackedByteArray data;
        
        protected:
            static void _bind_methods();
        
        public:
            static uint32_t get_header_length();

           

            
            PacketCPP();

            void generate(
                CmdType cmd_type, 
                HDWIType hdwi_type,
                uint64_t time_ns,
                PackedByteArray bytes);

            void generate_from_bytes(PackedByteArray header_bytes);
            ~PacketCPP();
            bool get_validity();

            int64_t get_cmd_id();
            int64_t get_type();
            uint64_t get_time_ns();
            static uint32_t get_header_len();
            uint32_t get_data_len();
            String _to_string() const;
            void set_data(PackedByteArray data);
            PackedByteArray get_data() const;
            PackedByteArray convert_to_bytes();

    };
}

VARIANT_ENUM_CAST(godot::CmdType);
VARIANT_ENUM_CAST(godot::HDWIType);


#endif