#ifndef PACKET_H
#define PACKET_H


#include "../HDWI/hdwi.h"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <cstdint>


namespace godot {

    
    #define VERSION 4

     enum class CmdType : uint8_t {
                CMD_SYNCH = 1,
                CMD_ACTION = 2
            };


    struct PacketHeader {
        uint16_t version;
        CmdType cmd_id;
        HDWIType type;
        uint8_t device_index;
        char reversed[3];
        uint64_t time_ns;
        uint32_t data_len;
    };





    class PacketCPP : public RefCounted {
        GDCLASS(PacketCPP, RefCounted)
        private:
            bool valid = false;
            PacketHeader header;
            PackedByteArray data;
        
        protected:
            static void _bind_methods();
        
        public:
            static uint32_t get_header_length();

           

            
            PacketCPP();

            void generate(uint8_t device_index, 
                CmdType cmd_type, 
                HDWIType hdwi_type,
                uint64_t time_ns,
                PackedByteArray bytes);

            void generate_from_bytes(PackedByteArray header_bytes);
            ~PacketCPP();
            bool get_validity();

            int64_t get_cmd_id();
            int64_t get_type();
            uint8_t get_device_index();
            uint64_t get_time_ns();
            static uint32_t get_header_len();
            uint32_t get_data_len();
            String _to_string() const;
            void set_data(PackedByteArray data);

    };
}

VARIANT_ENUM_CAST(godot::CmdType)
VARIANT_ENUM_CAST(godot::HDWIType)


#endif