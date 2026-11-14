# Shared helpers for Aero Desktop modules.
#
# Every module is a static library with a public include directory under
# <root>/include.  Keeping the public headers outside the source tree makes
# accidental private-header dependencies obvious during review.

set(AERO_PUBLIC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

function(aero_add_module target)
    cmake_parse_arguments(ARG "" "HEADER_DIR" "SOURCES;PUBLIC_LIBS;PRIVATE_LIBS" ${ARGN})

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "aero_add_module(${target}) requires SOURCES")
    endif()

    add_library(${target} STATIC ${ARG_SOURCES})
    add_library(Aero::${target} ALIAS ${target})

    # AUTOMOC only processes headers that are part of the target or that share
    # a base name with one of its sources.  Public headers live outside the
    # module source tree, so register them explicitly; otherwise Q_OBJECT
    # classes declared there would produce unresolved vtables/meta-objects.
    if(ARG_HEADER_DIR)
        file(GLOB_RECURSE AERO_MODULE_HEADERS CONFIGURE_DEPENDS
            "${AERO_PUBLIC_INCLUDE_DIR}/aero/${ARG_HEADER_DIR}/*.h")
        target_sources(${target} PRIVATE ${AERO_MODULE_HEADERS})
    endif()

    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${AERO_PUBLIC_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON
    )

    target_link_libraries(${target} PUBLIC ${ARG_PUBLIC_LIBS})
    if(ARG_PRIVATE_LIBS)
        target_link_libraries(${target} PRIVATE ${ARG_PRIVATE_LIBS})
    endif()
endfunction()
