if(BUILD_TESTING)
    enable_testing()

    add_executable(test_database
        test_database.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
    )
    target_include_directories(test_database PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_database PRIVATE Qt6::Core Qt6::Sql)
    target_compile_options(test_database PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
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
    target_compile_options(test_exporter PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_exporter COMMAND test_exporter)

    add_executable(test_lineweb_pusher
        test_lineweb_pusher.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/push/lineweb_pusher.cpp
    )
    target_include_directories(test_lineweb_pusher PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_lineweb_pusher PRIVATE Qt6::Core Qt6::Sql Qt6::Network Qt6::Test)
    target_compile_options(test_lineweb_pusher PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_lineweb_pusher COMMAND test_lineweb_pusher)

    add_executable(test_cloud_sync
        test_cloud_sync.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/push/lineweb_pusher.cpp
    )
    target_include_directories(test_cloud_sync PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_cloud_sync PRIVATE Qt6::Core Qt6::Sql Qt6::Network Qt6::Test)
    target_compile_options(test_cloud_sync PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_cloud_sync COMMAND test_cloud_sync)

    add_executable(test_window_tracker
        test_window_tracker.cpp
        ${CMAKE_SOURCE_DIR}/src/tracker/tracking_engine.cpp
    )
    target_include_directories(test_window_tracker PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_window_tracker PRIVATE Qt6::Core)
    target_compile_options(test_window_tracker PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_window_tracker COMMAND test_window_tracker)

    get_filename_component(QT_INSTALL_ROOT "${Qt6_DIR}/../../.." ABSOLUTE)
    get_filename_component(COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_tests_properties(
        test_database test_exporter test_lineweb_pusher test_cloud_sync test_window_tracker
        PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${QT_INSTALL_ROOT}/bin;PATH=path_list_prepend:${COMPILER_BIN_DIR}"
        ENVIRONMENT "QT_PLUGIN_PATH=${QT_INSTALL_ROOT}/plugins"
    )
endif()
