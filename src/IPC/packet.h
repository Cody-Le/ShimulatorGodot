#ifndef PACKET_H
#define PACKET_H


#include "sim_packet_type.h"
#include "../HDWI/hdwi_type.h"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <cstdint>


namespace godot {

    // Wire-protocol version stamped into every outbound header and required on
    // every inbound one. Bump in lockstep with the kernel driver (D049/D050/v5
    // multiprocess pid).
    #define VERSION 5

    // Header cmd_id values. SYNCH = time-seed + device registration body;
    // ACTION = peripheral I/O (dev_id + action payload); EVENT = reverse-channel
    // engine -> kernel async event; PROC_HELLO = process announce (pid -> name).
    enum class CmdType : uint8_t {
        SYNCH      = 0x01,
        ACTION     = 0x02,
        EVENT      = 0x03,
        PROC_HELLO = 0x04
    };

    // PacketCPP — encode/decode one simcall wire frame (header + payload).
    //
    // Frame layout on the wire:
    //   [ simcall_header_t (24B, little-endian, version 5) ][ payload (data_len B) ]
    //
    // header.pid identifies the originating FSW process (tgid) on forward frames;
    // it is 0 on engine -> kernel events. Replies MUST echo the request's pid
    // (see generate()) — the kernel logs a desync warning otherwise.
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

            // pid: for a reply, this MUST be the request's header.pid (echoed back
            // for the kernel's lockstep validation); for an unsolicited/event frame
            // pass 0.
            void generate(
                CmdType cmd_type,
                HDWIType hdwi_type,
                uint64_t time_ns,
                PackedByteArray bytes,
                uint32_t pid);

            void generate_from_bytes(PackedByteArray header_bytes);
            ~PacketCPP();
            bool get_validity();

            int64_t get_cmd_id();
            int64_t get_type();
            int64_t get_pid();
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