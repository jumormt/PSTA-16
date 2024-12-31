if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# psta mem_leak tests
set(cmd "psta -leak -spatial=false -wrapper -valid-tests -print-stat=false -stat=false -layer=0 ")
file(GLOB mem_leak_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/mem_leak/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${mem_leak_files})
    add_test(
            NAME mem_leak_c_0/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB mem_leak_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tmem_leak/*.bc*")
#foreach(filename ${mem_leak_files})
#    add_test(
#            NAME mem_leak_c_0/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()

# typestate mem_leak tests
set(cmd "psta -leak -wrapper -valid-tests -print-stat=false -stat=false -layer=0 ")
file(GLOB mem_leak_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/mem_leak/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${mem_leak_files})
  add_test(
      NAME mem_leak_c_1/${filename}
      COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
  )
endforeach()
#file(GLOB mem_leak_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tmem_leak/*.bc*")
#foreach(filename ${mem_leak_files})
#    add_test(
#            NAME mem_leak_c_1/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()

set(cmd "psta -uaf -wrapper -valid-tests -stat=false -spatial=false -print-stat=false -layer=0 ")
file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/uaf/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${uaf_files})
    add_test(
            NAME uaf_c_0/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tuaf/*.bc*")
#foreach(filename ${uaf_files})
#    add_test(
#            NAME uaf_c_0/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()


# typestate uaf tests
set(cmd "psta -uaf -wrapper -valid-tests -stat=false -print-stat=false -layer=0 ")
file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/uaf/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${uaf_files})
    add_test(
            NAME uaf_c_1/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tuaf/*.bc*")
#foreach(filename ${uaf_files})
#    add_test(
#            NAME uaf_c_1/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()


# typestate uaf multi slicing tests
set(cmd "psta -uaf -wrapper -mul  -valid-tests -stat=false -print-stat=false -layer=0 ")
file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/uaf/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${uaf_files})
    add_test(
            NAME uaf_c_m/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tuaf/*.bc*")
#foreach(filename ${uaf_files})
#    add_test(
#            NAME uaf_c_m/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()

# typestate df tests
set(cmd "psta -df -wrapper -valid-tests -stat=false -print-stat=false -layer=0 ")
file(GLOB df_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/df/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${df_files})
    add_test(
            NAME df_c/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB df_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tdf/*.bc*")
#foreach(filename ${df_files})
#    add_test(
#            NAME df_c/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()

# typestate mem_leak tests
set(cmd "psta -iso-summary -leak -spatial=false -wrapper -valid-tests -print-stat=false -stat=false -layer=0 ")
file(GLOB mem_leak_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/mem_leak/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${mem_leak_files})
    add_test(
            NAME iso_mem_leak_c_0/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB mem_leak_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tmem_leak/*.bc*")
#foreach(filename ${mem_leak_files})
#    add_test(
#            NAME iso_mem_leak_c_0/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()

# typestate mem_leak tests
set(cmd "psta -iso-summary -leak -wrapper -valid-tests -print-stat=false -stat=false -layer=0 ")
file(GLOB mem_leak_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/mem_leak/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${mem_leak_files})
    add_test(
            NAME iso_mem_leak_c_1/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB mem_leak_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tmem_leak/*.bc*")
#foreach(filename ${mem_leak_files})
#    add_test(
#            NAME iso_mem_leak_c_1/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()

set(cmd "psta -iso-summary -uaf -wrapper -valid-tests -stat=false -spatial=false -print-stat=false -layer=0 ")
file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/uaf/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${uaf_files})
    add_test(
            NAME iso_uaf_c_0/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tuaf/*.bc*")
#foreach(filename ${uaf_files})
#    add_test(
#            NAME iso_uaf_c_0/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()

# typestate uaf tests
set(cmd "psta -iso-summary -uaf -wrapper -valid-tests -stat=false -print-stat=false -layer=0 ")
file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/uaf/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${uaf_files})
    add_test(
            NAME iso_uaf_c_1/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tuaf/*.bc*")
#foreach(filename ${uaf_files})
#    add_test(
#            NAME iso_uaf_c_1/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()

# typestate uaf multi slicing tests
set(cmd "psta -iso-summary -uaf -wrapper -mul  -valid-tests -stat=false -print-stat=false -layer=0 ")
file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/uaf/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${uaf_files})
    add_test(
            NAME iso_uaf_c_m/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tuaf/*.bc*")
#foreach(filename ${uaf_files})
#    add_test(
#            NAME iso_uaf_c_m/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()

# typestate df tests
set(cmd "psta -iso-summary -df -wrapper -valid-tests -stat=false -print-stat=false -spatial=false -layer=0 ")
file(GLOB df_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/df/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${df_files})
    add_test(
            NAME iso_df_c_0/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB df_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tdf/*.bc*")
#foreach(filename ${df_files})
#    add_test(
#            NAME iso_df_c_0/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()

# typestate df tests
set(cmd "psta -iso-summary -df -wrapper -valid-tests -stat=false -print-stat=false -layer=0 ")
file(GLOB df_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/df/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${df_files})
    add_test(
            NAME iso_df_c_1/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB df_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tdf/*.bc*")
#foreach(filename ${df_files})
#    add_test(
#            NAME iso_df_c_1/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()

# typestate df tests
set(cmd "psta -iso-summary -df -wrapper -valid-tests -stat=false -print-stat=false -layer=0 -mul")
file(GLOB df_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/df/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${df_files})
    add_test(
            NAME iso_df_c_m/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB df_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tdf/*.bc*")
#foreach(filename ${df_files})
#    add_test(
#            NAME iso_df_c_m/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()


# Below is the tests for baseline detectors.
# ESP mem_leak tests
set(cmd "psta -leak -base=true -wrapper -valid-tests -print-stat=false -stat=false")
file(GLOB mem_leak_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/mem_leak/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${mem_leak_files})
    add_test(
            NAME base_mem_leak/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB mem_leak_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tmem_leak/*.bc*")
#foreach(filename ${mem_leak_files})
#    add_test(
#            NAME base_mem_leak/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()

# ESP uaf tests
set(cmd "psta -uaf -base=true -wrapper -valid-tests -stat=false -print-stat=false")
file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/uaf/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${uaf_files})
    add_test(
            NAME base_uaf/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB uaf_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tuaf/*.bc*")
#foreach(filename ${uaf_files})
#    add_test(
#            NAME base_uaf/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()


# typestate df tests
set(cmd "psta -df -base=true -wrapper -valid-tests -stat=false -print-stat=false")
file(GLOB df_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/df/*.bc*")
string(REPLACE " " ";" commandtemp ${cmd})
set(command ${commandtemp})
# loops over each file creating a test
foreach(filename ${df_files})
    add_test(
            NAME base_df/${filename}
            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
    )
endforeach()
#file(GLOB df_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/test_cases_bc/tdf/*.bc*")
#foreach(filename ${df_files})
#    add_test(
#            NAME base_df/${filename}
#            COMMAND ${command} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
#            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}-build/bin
#    )
#endforeach()