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

    add_executable(test_ai_client
        test_ai_client.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/ai/ai_client.cpp
    )
    target_include_directories(test_ai_client PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_ai_client PRIVATE Qt6::Core Qt6::Sql Qt6::Network Qt6::Test)
    target_compile_options(test_ai_client PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_ai_client COMMAND test_ai_client)

    add_executable(test_reminder_scheduler
        test_reminder_scheduler.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/ai/ai_client.cpp
        ${CMAKE_SOURCE_DIR}/src/reminder/reminder_scheduler.cpp
    )
    target_include_directories(test_reminder_scheduler PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_reminder_scheduler PRIVATE Qt6::Core Qt6::Sql Qt6::Network Qt6::Test)
    target_compile_options(test_reminder_scheduler PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_reminder_scheduler COMMAND test_reminder_scheduler)

    add_executable(test_weekly_report
        test_weekly_report.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/ai/ai_client.cpp
        ${CMAKE_SOURCE_DIR}/src/report/report_html_builder.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/theme_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/reminder/weekly_report_manager.cpp
    )
    target_include_directories(test_weekly_report PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_weekly_report PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Network Qt6::Test dwmapi)
    target_compile_options(test_weekly_report PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_weekly_report COMMAND test_weekly_report)

    add_executable(test_daily_report
        test_daily_report.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/ai/ai_client.cpp
        ${CMAKE_SOURCE_DIR}/src/report/report_html_builder.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/theme_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/report/daily_report_manager.cpp
    )
    target_include_directories(test_daily_report PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_daily_report PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Network Qt6::Test dwmapi)
    target_compile_options(test_daily_report PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_daily_report COMMAND test_daily_report)

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

    add_executable(test_trend_chart_layout
        test_trend_chart_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/trend_chart_layout.cpp
    )
    target_include_directories(test_trend_chart_layout PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_trend_chart_layout PRIVATE Qt6::Core Qt6::Gui)
    target_compile_options(test_trend_chart_layout PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_trend_chart_layout COMMAND test_trend_chart_layout)

    add_executable(test_dashboard_layout
        test_dashboard_layout.cpp
    )
    target_include_directories(test_dashboard_layout PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_dashboard_layout PRIVATE Qt6::Core)
    target_compile_options(test_dashboard_layout PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_dashboard_layout COMMAND test_dashboard_layout)

    add_executable(test_period_distribution_layout
        test_period_distribution_layout.cpp
    )
    target_include_directories(test_period_distribution_layout PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_period_distribution_layout PRIVATE Qt6::Core)
    target_compile_options(test_period_distribution_layout PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_period_distribution_layout COMMAND test_period_distribution_layout)

    add_executable(test_rank_layout
        test_rank_layout.cpp
    )
    target_include_directories(test_rank_layout PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_rank_layout PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
    target_compile_options(test_rank_layout PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_rank_layout COMMAND test_rank_layout)

    add_executable(test_theme_manager
        test_theme_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/theme_manager.cpp
    )
    target_include_directories(test_theme_manager PRIVATE ${CMAKE_SOURCE_DIR}/src)
    # theme_manager.cpp 的标题栏工具使用 DwmSetWindowAttribute。
    target_link_libraries(test_theme_manager PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql dwmapi)
    target_compile_options(test_theme_manager PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_theme_manager COMMAND test_theme_manager)

    add_executable(test_update_checker
        test_update_checker.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/update/update_checker.cpp
    )
    target_include_directories(test_update_checker PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_update_checker PRIVATE Qt6::Core Qt6::Sql Qt6::Network Qt6::Test)
    target_compile_definitions(test_update_checker PRIVATE APP_VERSION="${PROJECT_VERSION}")
    target_compile_options(test_update_checker PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_update_checker COMMAND test_update_checker)

    add_executable(test_trend_card_geometry
        test_trend_card_geometry.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/card_frame.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/theme_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/trend_chart_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/trend_card.cpp
    )
    target_include_directories(test_trend_card_geometry PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_trend_card_geometry PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql dwmapi)
    target_compile_options(test_trend_card_geometry PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
    add_test(NAME test_trend_card_geometry COMMAND test_trend_card_geometry)

    get_filename_component(QT_INSTALL_ROOT "${Qt6_DIR}/../../.." ABSOLUTE)
    get_filename_component(COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_tests_properties(
        test_database test_exporter test_lineweb_pusher test_cloud_sync test_ai_client test_reminder_scheduler test_weekly_report test_daily_report test_window_tracker test_trend_chart_layout test_dashboard_layout test_period_distribution_layout test_rank_layout test_trend_card_geometry test_theme_manager test_update_checker
        PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${QT_INSTALL_ROOT}/bin;PATH=path_list_prepend:${COMPILER_BIN_DIR}"
        ENVIRONMENT "QT_PLUGIN_PATH=${QT_INSTALL_ROOT}/plugins"
    )
endif()
