file(GLOB SOURCES
        Detectors/*.cpp
        Slicing/*.cpp
        PSTA/*.cpp
        )

add_library(PstaCore SHARED ${SOURCES})
target_link_libraries(PstaCore ${SVF_LIB} ${llvm_libs} ${Z3_LIBRARIES})
set_target_properties(PstaCore PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        )
install(TARGETS PstaCore
        LIBRARY DESTINATION lib    # 对于 Linux/macOS 动态库
        ARCHIVE DESTINATION lib    # 对于静态库
        )