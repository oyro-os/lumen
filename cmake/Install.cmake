# Installation rules

# Installation directories
include(GNUInstallDirs)

# Install libraries with export
if(BUILD_STATIC)
    install(TARGETS lumen_static
        EXPORT LumenTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(BUILD_SHARED_RELEASE)
    install(TARGETS lumen_shared_release
        EXPORT LumenTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

# Install public headers
install(FILES ${LUMEN_PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lumen
)

# Install generated version header
install(FILES ${CMAKE_BINARY_DIR}/include/lumen/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lumen
)

# Install CMake config files
install(EXPORT LumenTargets
    FILE LumenTargets.cmake
    NAMESPACE Lumen::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Lumen
)

# Create and install package config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/LumenConfig.cmake.in
    ${CMAKE_BINARY_DIR}/LumenConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Lumen
)

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/LumenConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_BINARY_DIR}/LumenConfig.cmake
    ${CMAKE_BINARY_DIR}/LumenConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Lumen
)

# Install pkg-config file
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/lumen.pc.in
    ${CMAKE_BINARY_DIR}/lumen.pc
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/lumen.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Generate unified C header for distribution
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/dist/${LUMEN_PLATFORM}/${LUMEN_ARCH}/lumen.h
    COMMAND ${CMAKE_COMMAND} -E copy 
        ${CMAKE_SOURCE_DIR}/include/lumen/lumen.h
        ${CMAKE_SOURCE_DIR}/dist/${LUMEN_PLATFORM}/${LUMEN_ARCH}/lumen.h
    DEPENDS ${CMAKE_SOURCE_DIR}/include/lumen/lumen.h
    COMMENT "Copying header to dist directory"
)

add_custom_target(generate_headers ALL
    DEPENDS ${CMAKE_SOURCE_DIR}/dist/${LUMEN_PLATFORM}/${LUMEN_ARCH}/lumen.h
)

# Create platform-specific package
set(CPACK_PACKAGE_NAME "lumen")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lumen Embedded Database Engine")
set(CPACK_PACKAGE_VENDOR "Lumen Project")

if(LUMEN_PLATFORM STREQUAL "windows")
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

set(CPACK_PACKAGE_FILE_NAME "lumen-${PROJECT_VERSION}-${LUMEN_PLATFORM}-${LUMEN_ARCH}")

include(CPack)