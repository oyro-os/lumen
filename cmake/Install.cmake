# Installation rules

# Install libraries
if(BUILD_STATIC)
    install(TARGETS lumen_static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endif()

if(BUILD_SHARED_RELEASE)
    install(TARGETS lumen_shared_release
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
endif()

# Install public headers
install(FILES ${LUMEN_PUBLIC_HEADERS}
    DESTINATION include/lumen
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