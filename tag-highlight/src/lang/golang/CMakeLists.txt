# Golang

if (NOT CMAKE_GOLANG_EXECUTABLE)
    find_program(CMAKE_GOLANG_EXECUTABLE NAMES go)
endif()

if (CMAKE_GOLANG_EXECUTABLE)
    set (golang_SOURCES go_src/main.go go_src/sock.go)
    set (golang_TARGET  golang) 
    set (golang_OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/${golang_TARGET}")
    set (golang_MODDIR  "${CMAKE_CURRENT_SOURCE_DIR}/go_src")

    separate_arguments(REAL_GOLANG_FLAGS NATIVE_COMMAND ${CMAKE_GOLANG_FLAGS})
    add_custom_command(
        OUTPUT "${golang_OUTPUT}"
        DEPENDS ${golang_SOURCES}
        WORKING_DIRECTORY ${golang_MODDIR}
        COMMAND ${CMAKE_GOLANG_EXECUTABLE} build
                -o "${golang_OUTPUT}"
                ${REAL_GOLANG_FLAGS}
                
        COMMENT "Building Go module."
        VERBATIM
    )
    add_custom_target(golang ALL DEPENDS "${golang_OUTPUT}")

    install(PROGRAMS "${golang_OUTPUT}"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
else()
    message(WARNING "Go executable not found. Cannot build Go module.")
endif()
