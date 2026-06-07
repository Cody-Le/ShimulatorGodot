#ifndef PACKET_H
#define PACKET_H


#include "sim_packet_type.h"
#include "../HDWI/hdwi_type.h"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <cstdint>


namespace godot {

    // Wire-protocol version stamped into every outbound header and required on
    // every inbound one. Bump in lockstep with the kernel driver (D049/D050).
    #define VERSION 4

    // Header cmd_id values. SYNCH = time-seed + device registration body;
    // ACTION = peripheral I/O (dev_id + action payload).
    enum class CmdType : uint8_t {
        SYNCH  = 0x01,
        ACTION = 0x02
    };

    // PacketCPP — encode/decode one simcall wire frame (header + payload).
    //
    // Frame layout on the wire:
    //   [ simcall_header_t (24B, little-endian, version 4) ][ payload (data_len B) ]
    //
    // Outbound: generate(...) then convert_to_bytes() and write to the socket.
    //   HDWI resources do this internally and emit the bytes via on_send.
    // Inbound:  read get_header_len() bytes, generate_from_bytes(...), check
    //   get_validity(), then read get_data_len() more bytes for the body.
    //
    // Full class reference (members, methods, examples) lives in
    // doc_classes/PacketCPP.xml and shows up in the Godot editor.

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
            String to_string_robust() const;
            void set_robust(bool enabled);
            bool get_robust() const;
            void set_data(PackedByteArray data);
            PackedByteArray get_data() const;
            PackedByteArray convert_to_bytes();

        private:
            bool robust = false;
            String build_robust() const;

    };
}

VARIANT_ENUM_CAST(godot::CmdType);
VARIANT_ENUM_CAST(godot::HDWIType);


#endif