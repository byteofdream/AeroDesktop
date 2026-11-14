aero_add_module(AeroGraphics
    HEADER_DIR graphics
    SOURCES
        backdrop_provider.cpp
        glass_panel.cpp
        wallpaper.cpp
    PUBLIC_LIBS
        AeroCore
        AeroTheme
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
