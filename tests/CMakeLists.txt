set(MICROVPR_TESTS
    test_blif_parser
    test_netlist_evaluator
    test_tech_mapper
)

foreach(test_name ${MICROVPR_TESTS})
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE microvpr_core)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
