aero_add_module(AeroTheme
    HEADER_DIR theme
    SOURCES
        theme_manager.cpp
        style.cpp
        icon_factory.cpp
        controls.cpp
    PUBLIC_LIBS
        AeroCore
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
