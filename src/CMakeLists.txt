set(SOURCES
    main.cpp
    database/database_manager.cpp
    tracker/window_tracker.cpp
    tracker/tracking_engine.cpp
    icon/app_icon_provider.cpp
    ai/ai_client.cpp
    ui/main_window.cpp
    ui/settings_dialog.cpp
    ui/theme_manager.cpp
    ui/tray_manager.cpp
    ui/card_frame.cpp
    ui/hero_card.cpp
    ui/trend_chart_layout.cpp
    ui/trend_card.cpp
    ui/rank_card.cpp
    ui/ai_report_card.cpp
    report/report_html_builder.cpp
    report/daily_report_manager.cpp
    export/exporter.cpp
    export/xlsx_writer.cpp
    utility/autostart_helper.cpp
    push/lineweb_pusher.cpp
    reminder/reminder_scheduler.cpp
    reminder/weekly_report_manager.cpp
    ${CMAKE_SOURCE_DIR}/third_party/miniz/miniz_all.cpp
)

set(HEADERS
    database/database_manager.h
    tracker/window_tracker.h
    tracker/tracking_engine.h
    tracker/tracking_store.h
    icon/app_icon_provider.h
    ai/ai_client.h
    ui/main_window.h
    ui/settings_dialog.h
    ui/settings_icons.h
    ui/theme_manager.h
    ui/tray_manager.h
    ui/design_tokens.h
    ui/dashboard_layout.h
    ui/period_distribution_layout.h
    ui/ui_utils.h
    ui/card_frame.h
    ui/hero_card.h
    ui/trend_chart_layout.h
    ui/trend_card.h
    ui/rank_card.h
    ui/rank_layout.h
    ui/ai_report_card.h
    report/report_html_builder.h
    report/daily_report_manager.h
    export/exporter.h
    export/xlsx_writer.h
    utility/autostart_helper.h
    utility/process_identity.h
    push/lineweb_pusher.h
    reminder/reminder_scheduler.h
    reminder/weekly_report_manager.h
)

qt6_add_resources(RESOURCES ${CMAKE_SOURCE_DIR}/resources/resources.qrc)

set(RESOURCE_RC ${CMAKE_SOURCE_DIR}/resources/icon.rc)
set(RESOURCE_RES ${CMAKE_CURRENT_BINARY_DIR}/icon.res)
set(RESOURCE_VERSION "${PROJECT_VERSION}")
set(RESOURCE_VERSION_NUM "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")

# 注意:windres 的 -I 不能传带空格的引号路径(预处理阶段会被拆断),这里把工作目录指向
# resources 目录,icon.rc 里的相对引用 icon.ico 直接从当前目录解析,不传任何 -I。
# 版本宏不带引号(避免 popen 转义问题),字符串值由 icon.rc 内 STR1() 宏展开生成。
# -l 0x804 把资源语言设为简体中文,否则默认 0x0409 与 icon.rc 内 080404b0 块名不匹配,
# GetFileVersionInfoSizeW 会找不到版本资源。
add_custom_command(
    OUTPUT ${RESOURCE_RES}
    COMMAND windres -l 0x804 --define APP_VERSION=${RESOURCE_VERSION} --define APP_VERSION_NUM=${RESOURCE_VERSION_NUM} -i "${RESOURCE_RC}" -O coff -o "${RESOURCE_RES}"
    DEPENDS ${RESOURCE_RC}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/resources
    COMMENT "Compiling Windows resource file"
)

# .res 是 COFF 对象。Ninja 生成器下必须同时满足两点资源才会被链接进 exe:
# 1) add_executable 显式列出 .res,否则不会生成 CUSTOM_COMMAND 规则(ninja 报 no rule to make);
# 2) target_link_libraries 再链接一次,否则 exe 没有 .rsrc 段(版本信息/图标都会缺失)。
add_executable(TimeMaster WIN32 ${SOURCES} ${HEADERS} ${RESOURCES} ${RESOURCE_RES})

target_compile_definitions(TimeMaster PRIVATE MINIZ_STATIC_DEFINE)

target_compile_definitions(TimeMaster PRIVATE APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(TimeMaster PRIVATE ${RESOURCE_RES})

target_include_directories(TimeMaster PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(TimeMaster PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(TimeMaster PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Svg
    Qt6::Network
    dwmapi
    shell32
    gdi32
    version
)
