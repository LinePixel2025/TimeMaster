set(SOURCES
    main.cpp
    database/database_manager.cpp
    tracker/window_tracker.cpp
    icon/app_icon_provider.cpp
    ui/main_window.cpp
    ui/stats_widget.cpp
    ui/app_rank_widget.cpp
    ui/settings_dialog.cpp
    ui/theme_manager.cpp
    ui/tray_manager.cpp
    ui/chart_card.cpp
    ui/compare_card.cpp
    ui/dashboard_card.cpp
    ui/dashboard_layout.cpp
    ui/dashboard_widget.cpp
    ui/grid_editor.cpp
    ui/hero_card.cpp
    ui/insight_card.cpp
    ui/rank_card.cpp
    ui/svg_icon.cpp
    ui/topapp_card.cpp
    ui/wallpaper_helper.cpp
    export/exporter.cpp
    export/xlsx_writer.cpp
    utility/autostart_helper.cpp
    push/lineweb_pusher.cpp
    ${CMAKE_SOURCE_DIR}/third_party/miniz/miniz_all.cpp
)

set(HEADERS
    database/database_manager.h
    tracker/window_tracker.h
    icon/app_icon_provider.h
    ui/main_window.h
    ui/stats_widget.h
    ui/app_rank_widget.h
    ui/settings_dialog.h
    ui/theme_manager.h
    ui/tray_manager.h
    ui/chart_card.h
    ui/compare_card.h
    ui/dashboard_card.h
    ui/dashboard_layout.h
    ui/dashboard_widget.h
    ui/design_tokens.h
    ui/grid_editor.h
    ui/hero_card.h
    ui/insight_card.h
    ui/rank_card.h
    ui/svg_icon.h
    ui/topapp_card.h
    ui/wallpaper_helper.h
    export/exporter.h
    export/xlsx_writer.h
    utility/autostart_helper.h
    push/lineweb_pusher.h
)

qt6_add_resources(RESOURCES ${CMAKE_SOURCE_DIR}/resources/resources.qrc)

set(RESOURCE_RC ${CMAKE_SOURCE_DIR}/resources/icon.rc)
set(RESOURCE_RES ${CMAKE_CURRENT_BINARY_DIR}/icon.res)

add_custom_command(
    OUTPUT ${RESOURCE_RES}
    COMMAND windres -i "${RESOURCE_RC}" -O coff -o "${RESOURCE_RES}"
    DEPENDS ${RESOURCE_RC}
    COMMENT "Compiling Windows resource file"
)

add_executable(TimeMaster WIN32 ${SOURCES} ${HEADERS} ${RESOURCES} ${RESOURCE_RES})

target_compile_definitions(TimeMaster PRIVATE MINIZ_STATIC_DEFINE)

target_compile_definitions(TimeMaster PRIVATE APP_VERSION="${PROJECT_VERSION}")

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
)
