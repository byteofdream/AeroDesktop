aero_add_module(AeroCore
    HEADER_DIR core
    SOURCES
        logging.cpp
        paths.cpp
        json_config.cpp
        event_bus.cpp
        utils.cpp
    PUBLIC_LIBS
        Qt6::Core
        Qt6::Gui
)
