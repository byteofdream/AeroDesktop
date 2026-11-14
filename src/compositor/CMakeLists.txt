aero_add_module(AeroCompositor
    HEADER_DIR compositor
    SOURCES
        workspace_model.cpp
        window.cpp
        window_manager.cpp
    PUBLIC_LIBS
        AeroCore
        AeroTheme
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
