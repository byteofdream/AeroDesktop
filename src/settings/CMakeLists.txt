aero_add_module(AeroSettings
    HEADER_DIR settings
    SOURCES
        settings_manager.cpp
    PUBLIC_LIBS
        AeroCore
        Qt6::Core
        Qt6::Gui
)
