aero_add_module(AeroServices
    HEADER_DIR services
    SOURCES
        notification_bus.cpp
        calendar_store.cpp
        search_service.cpp
    PUBLIC_LIBS
        AeroCore
        AeroPlatform
        AeroSettings
        Qt6::Core
        Qt6::Gui
    PRIVATE_LIBS
        Qt6::Concurrent
)
