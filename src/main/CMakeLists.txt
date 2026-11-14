add_executable(aero-desktop main.cpp)

target_link_libraries(aero-desktop PRIVATE
    AeroShell
    AeroApplications
    AeroGraphics
    AeroCompositor
    AeroServices
    AeroSettings
    AeroTheme
    AeroPlatform
    AeroCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

target_compile_definitions(aero-desktop PRIVATE
    AERO_VERSION_STRING="${PROJECT_VERSION}"
)

set_target_properties(aero-desktop PROPERTIES
    OUTPUT_NAME aero-desktop
    WIN32_EXECUTABLE OFF
)
