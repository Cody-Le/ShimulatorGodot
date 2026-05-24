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
        
        protected:
            static void _bind_methods();
        
        public:
            // Signals
            // Component - (HDWI) -> CommSeq
            void on_send();
            // Methods
            // Comm - (Device Registry -> Component's HDWI Resource) -> HDWI
            void dispatch_action();
            
            HDWIResource() = default;

    };
}




#endif