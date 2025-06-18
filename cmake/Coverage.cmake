# Coverage configuration

function(setup_coverage target)
    if(NOT COVERAGE)
        return()
    endif()
    
    # Find coverage tools
    find_program(GCOV_EXECUTABLE gcov)
    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)
    
    if(NOT GCOV_EXECUTABLE OR NOT LCOV_EXECUTABLE OR NOT GENHTML_EXECUTABLE)
        message(WARNING "Coverage tools not found, coverage report disabled")
        message(STATUS "  gcov: ${GCOV_EXECUTABLE}")
        message(STATUS "  lcov: ${LCOV_EXECUTABLE}")
        message(STATUS "  genhtml: ${GENHTML_EXECUTABLE}")
        return()
    endif()
    
    # Create coverage target
    add_custom_target(coverage
        COMMAND ${LCOV_EXECUTABLE} --directory . --zerocounters
        COMMAND $<TARGET_FILE:${target}>
        COMMAND ${LCOV_EXECUTABLE} --directory . --capture --output-file coverage.info
        COMMAND ${LCOV_EXECUTABLE} --remove coverage.info '/usr/*' '*/third_party/*' '*/tests/*' --output-file coverage.info.cleaned
        COMMAND ${GENHTML_EXECUTABLE} -o coverage coverage.info.cleaned
        COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated in coverage/index.html"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS ${target}
        COMMENT "Generating coverage report"
    )
    
    # Create coverage-clean target
    add_custom_target(coverage-clean
        COMMAND ${LCOV_EXECUTABLE} --directory . --zerocounters
        COMMAND ${CMAKE_COMMAND} -E remove_directory coverage
        COMMAND ${CMAKE_COMMAND} -E remove coverage.info coverage.info.cleaned
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Cleaning coverage data"
    )
endfunction()