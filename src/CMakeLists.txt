file(GLOB SOURCES
        *.cpp
        Detectors/*.cpp
        Slicing/*.cpp
        PSTA/*.cpp
        )
add_executable(psta ${SOURCES})

target_link_libraries(psta ${SVF_LIB} ${llvm_libs} ${Z3_LIBRARIES})
set_target_properties(psta PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)