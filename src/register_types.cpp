#include "register_types.h"

#include <godot_cpp/godot.hpp>

#include "mp4_player.h"
using namespace godot;

void initialize_mp4_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    ClassDB::register_class<MP4Player>();
}

void uninitialize_mp4_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C" {

    GDExtensionBool GDE_EXPORT mp4ffmpeg_library_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        const GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization* r_initialization) {

        godot::GDExtensionBinding::InitObject init_obj(
            p_get_proc_address,
            p_library,
            r_initialization);

        init_obj.register_initializer(initialize_mp4_module);
        init_obj.register_terminator(uninitialize_mp4_module);
        init_obj.set_minimum_library_initialization_level(
            MODULE_INITIALIZATION_LEVEL_SCENE);

        return init_obj.init();
    }

}