#ifndef HDWI_H
#define HDWI_H

#include <cstdint>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/resource.hpp>
#include "../IPC/packet.h"



namespace godot {

    class HDWIResource : public Resource {
        GDCLASS(HDWIResource, Resource)
        private:
            // No member variables needed for now, but can be added later if necessary
            HDWIType type;
        
        protected:
            static void _bind_methods();
        
        public:
            // Device name @export variable, setter and getters
            String device_name;
            void set_device_name(const String &p_device_name);
            String get_device_name() const;

            // Getter for device type
            HDWIType get_type() {
                return type;
            };

            // Get device representation as a packed byte array
            virtual PackedByteArray get_device_representation() const;

            // Signals
            // Component - (HDWI) -> CommSeq
            void on_send();

            // Methods
            // Comm - (Device Registry -> Component's HDWI Resource) -> HDWI
            virtual void dispatch_action(PackedByteArray request_data) = 0;
            
            HDWIResource() = default;

    };
}




#endif