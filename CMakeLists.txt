cmake_minimum_required(VERSION 3.21)

project(GeSiER VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Sql PrintSupport)

qt_add_executable(GeSiER WIN32
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    coveragedashboard.cpp coveragedashboard.h
    traceabilitywidget.cpp traceabilitywidget.h
    n2matrixwidget.cpp n2matrixwidget.h
    databasemigrator.cpp databasemigrator.h
    csvutility.cpp csvutility.h
    req_sqlmanager.cpp req_sqlmanager.h
    sqltableform.cpp sqltableform.h sqltableform.ui
    tableviewmanager.cpp tableviewmanager.h
    sqltreemodel.cpp sqltreemodel.h
    sqltreereqmodel.cpp sqltreereqmodel.h
    sqltreeitem.cpp sqltreeitem.h
    reqtrackform.cpp reqtrackform.h reqtrackform.ui
    editrequirement.cpp editrequirement.h editrequirement.ui
    edittabledialog.cpp edittabledialog.h edittabledialog.ui
    edittableviewform.cpp edittableviewform.h edittableviewform.ui
    exportcsvdialog.cpp exportcsvdialog.h exportcsvdialog.ui
    importrequirementdialog.cpp importrequirementdialog.h importrequirementdialog.ui
    importbasicdialog.cpp importbasicdialog.h importbasicdialog.ui
    importlogdialog.cpp importlogdialog.h importlogdialog.ui
    sqlproxyrelationaldelegate.h
    qwidgetsettings.cpp qwidgetsettings.h
    qressources.qrc
)

target_link_libraries(GeSiER PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::PrintSupport
)
target_include_directories(GeSiER PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(GeSiER PRIVATE /W4)
else()
    target_compile_options(GeSiER PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(database_migrator_test
        tests/database_migrator_test.cpp
        databasemigrator.cpp databasemigrator.h
        sqltreemodel.cpp sqltreemodel.h
        sqltreeitem.cpp sqltreeitem.h
    )
    target_link_libraries(database_migrator_test PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
    target_include_directories(database_migrator_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME database_migrator COMMAND database_migrator_test -txt)
endif()

install(TARGETS GeSiER
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
