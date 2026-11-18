cmake_minimum_required(VERSION 3.19)
project(parkovi_ozera VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Sql)

qt_standard_project_setup()

qt_add_executable(parkovi_ozera
    WIN32 MACOSX_BUNDLE
    main.cpp
    style.h style.cpp
    database.h database.cpp
    auth.h auth.cpp
    logindialog.h logindialog.cpp
    mainwindow.h mainwindow.cpp
    dashboardpage.h dashboardpage.cpp
    tablepage.h tablepage.cpp
    bureautablemodel.h bureautablemodel.cpp
    bureaudelegate.h bureaudelegate.cpp
    recorddialog.h recorddialog.cpp
    reportmodel.h reportmodel.cpp
    invoicedialog.h invoicedialog.cpp
    reportspage.h reportspage.cpp
)

# Іконка exe для Windows (файл app.rc підключає img/app.ico)
if(WIN32)
    target_sources(parkovi_ozera PRIVATE app.rc)
endif()

# SQL-скрипти та логотип ЖК вбудовуються в exe.
qt_add_resources(parkovi_ozera "app_resources"
    PREFIX "/"
    FILES
        sql/schema.sql
        sql/seed.sql
        img/logo.png
        img/icon.png
)

# Вихідні файли в UTF-8 (кирилиця в рядках)
if(MSVC)
    target_compile_options(parkovi_ozera PRIVATE /utf-8)
endif()

target_link_libraries(parkovi_ozera
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Sql
)

include(GNUInstallDirs)
install(TARGETS parkovi_ozera
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET parkovi_ozera
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
