#ifndef HDWI_H
#define HDWI_H

#include <cstdint>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/resource.hpp>
#include "../IPC/packet.h"



namespace godot {

    // HDWIResource — abstract base for every virtualized peripheral (GPIO, SPI,
    // 1-Wire, ...). One instance == one device the unmodified FSW talks to.
    //
    // Flow: kernel driver forwards a hardware syscall over TCP -> device registry
    // routes the inner payload to the matching resource's dispatch_action() ->
    // the resource mutates state and/or emits a type-specific signal so GDScript
    // can react -> any response is framed and emitted via on_send for the comm
    // layer to write back to the socket.
    //
    // Subclass contract:
    //   - init()  : register this instance in the subclass's static device table
    //   - clear() : deregister it
    //   - dispatch_action(payload) : decode action byte + data, run the handler
    //   - get_device_representation() : append type-specific fields after the
    //     base 32-byte device_name for the CMD_SYNCH registration body
    //
    // Per-type usage + examples: doc_classes/*.xml (visible in the Godot editor).
    class HDWIResource : public Resource {
        GDCLASS(HDWIResource, Resource)
        private:
            // No member variables needed for now, but can be added later if necessary
            HDWIType type;
        
        protected:
            static void _bind_methods();
        
        public:
            // Last sim time seeded from any inbound kernel packet. Updated by GDScript
            // via set_sim_time_ns() on every received packet so that all outbound
            // responses carry a non-zero time_ns before D043 clocksource lands.
            static uint64_t sim_time_ns;
            static void     set_sim_time_ns(uint64_t t) { sim_time_ns = t; }
            static uint64_t get_sim_time_ns()           { return sim_time_ns; }

            // Device name @export variable, setter and getters
            String device_name;
            void set_device_name(const String &p_device_name);
            String get_device_name() const;

            // Getter for device type
            HDWIType get_type() {
                return type;
            };

            virtual void init() = 0;
            virtual void clear() = 0;


            // Get device representation as a packed byte array
            virtual PackedByteArray get_device_representation() const;

            // Signals
            // Component - (HDWI) -> CommSeq
            void on_send();

            // 

            // Methods
            // Comm - (Device Registry -> Component's HDWI Resource) -> HDWI
            virtual void dispatch_action(PackedByteArray request_data) = 0;
            
            HDWIResource() = default;

    };
}




#endif