#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <funnel.h>

#include "funnel_gd.h"
#include "funnel_sender.h"

using namespace godot;

void initialize_gdextension_types(ModuleInitializationLevel p_level)
{
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	
	GDREGISTER_CLASS(FunnelSender);
	int ret = funnel_init(&ctx);
	ERR_FAIL_COND_MSG(ret != 0, "[libfunnel] Unable to connect to pipewire");
	UtilityFunctions::print("[libfunnel] pipewire context initialized");
}

void uninitialize_gdextension_types(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	if (ctx != nullptr) {
    	funnel_shutdown(ctx);
		UtilityFunctions::print("[libfunnel] pipewire context initialized");
	}
}

extern "C"
{
	// Initialization
	GDExtensionBool GDE_EXPORT funnel_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization)
	{
		GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
		init_obj.register_initializer(initialize_gdextension_types);
		init_obj.register_terminator(uninitialize_gdextension_types);
		init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

		return init_obj.init();
	}
}