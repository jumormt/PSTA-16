file(GLOB SOURCES
        Detectors/*.cpp
        Slicing/*.cpp
        PSTA/*.cpp
        Bases/*.cpp
        )

add_library(PstaCore SHARED ${SOURCES})
if(DEFINED ZSTD_LIBRARIES)
    message("linking PstaCore to: ${SVF_LIB},  ${llvm_libs},  ${Z3_LIBRARIES},  ${ZSTD_LIBRARIES}")
    target_link_libraries(PstaCore ${SVF_LIB} ${llvm_libs} ${Z3_LIBRARIES} ${ZSTD_LIBRARIES})
else()
    message("linking PstaCore to: ${SVF_LIB},  ${llvm_libs},  ${Z3_LIBRARIES}")
    target_link_libraries(PstaCore ${SVF_LIB} ${llvm_libs} ${Z3_LIBRARIES})
endif()

set_target_properties(PstaCore PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        )
# 确保install之后psta可以找到SVF和LLVM的lib
message("set PstaCore rpath to: ${SVF_INSTALL_LIB_DIR};${LLVM_LIBRARY_DIRS}")
set_target_properties(PstaCore PROPERTIES INSTALL_RPATH "${SVF_INSTALL_LIB_DIR};${LLVM_LIBRARY_DIRS}")
install(TARGETS PstaCore
        LIBRARY DESTINATION lib    # 对于 Linux/macOS 动态库
        ARCHIVE DESTINATION lib    # 对于静态库
        )