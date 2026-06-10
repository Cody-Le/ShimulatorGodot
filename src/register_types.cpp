#include "register_types.h"
#include "IPC/packet.h"
#include "HDWI/hdwi.h"
#include "HDWI/gpio/hdwi_gpio.h"
#include "HDWI/spi/hdwi_spi.h"
#include "HDWI/i2c/hdwi_i2c.h"
#include "HDWI/onewire/hdwi_onewire.h"
#include "HDWI/uart/hdwi_uart.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;


void initialize_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(PacketCPP);
    GDREGISTER_ABSTRACT_CLASS(HDWIResource);
    GDREGISTER_CLASS(HDWIGPIOResource);
    GDREGISTER_CLASS(HDWISPIResource);
    GDREGISTER_CLASS(HDWII2CResource);
    GDREGISTER_CLASS(HDWIOneWireResource);
    GDREGISTER_CLASS(HDWIUARTResource);

}

void uninitialize_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {
// Initialization.
    GDExtensionBool GDE_EXPORT sim_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
        godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

        init_obj.register_initializer(initialize_module);
        init_obj.register_terminator(uninitialize_module);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

        return init_obj.init();
    }
}

