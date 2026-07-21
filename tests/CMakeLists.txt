if(BUILD_TESTING)
    enable_testing()

    add_executable(test_database
        test_database.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
    )
    target_include_directories(test_database PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_database PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME test_database COMMAND test_database)

    add_executable(test_exporter
        test_exporter.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/export/exporter.cpp
        ${CMAKE_SOURCE_DIR}/src/export/xlsx_writer.cpp
        ${CMAKE_SOURCE_DIR}/third_party/miniz/miniz_all.cpp
    )
    target_compile_definitions(test_exporter PRIVATE MINIZ_STATIC_DEFINE)
    target_include_directories(test_exporter PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(test_exporter PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME test_exporter COMMAND test_exporter)

    add_executable(test_lineweb_pusher
        test_lineweb_pusher.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/push/lineweb_pusher.cpp
    )
    target_include_directories(test_lineweb_pusher PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_lineweb_pusher PRIVATE Qt6::Core Qt6::Sql Qt6::Network Qt6::Test)
    add_test(NAME test_lineweb_pusher COMMAND test_lineweb_pusher)
endif()
