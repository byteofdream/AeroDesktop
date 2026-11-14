aero_add_module(AeroPlatform
    HEADER_DIR platform
    SOURCES
        platform.cpp
        platform_service.cpp
        platform_factory.cpp
        linux/linux_platform.cpp
        linux/linux_file_system.cpp
        linux/linux_display_manager.cpp
        linux/linux_input_manager.cpp
        linux/linux_process_manager.cpp
        linux/linux_network_manager.cpp
        linux/linux_bluetooth_manager.cpp
        linux/linux_audio_manager.cpp
        linux/linux_power_manager.cpp
        linux/linux_notification_service.cpp
        linux/linux_session_manager.cpp
        linux/linux_settings_backend.cpp
        linux/linux_window_manager.cpp
        dreamyos/dreamyos_platform.cpp
    PUBLIC_LIBS
        AeroCore
        Qt6::Core
        Qt6::Gui
    PRIVATE_LIBS
        Qt6::Network
        Qt6::DBus
        Qt6::Concurrent
)

target_include_directories(AeroPlatform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
