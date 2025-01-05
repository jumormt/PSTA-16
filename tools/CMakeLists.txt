
add_executable(psta psta.cpp)

target_link_libraries(psta PstaCore)
set_target_properties(psta PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set_target_properties(psta PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;${SVF_INSTALL_LIB_DIR}")

message("set lib dir to: ${CMAKE_INSTALL_PREFIX}/lib ${SVF_INSTALL_LIB_DIR}")

install(
        TARGETS psta
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
