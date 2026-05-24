#ifndef HDWI_GPIO_H
#define HDWI_GPIO_H

#include "../hdwi.h"

namespace godot {

    class HDWIGPIOResource : public HDWIResource {
        GDCLASS(HDWIGPIOResource, HDWIResource)
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

            
            HDWIGPIOResource() = default;

    };

}


#endif