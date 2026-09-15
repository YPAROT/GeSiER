cmake_minimum_required(VERSION 3.21)

project(GeSiER VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(QT_NO_PRIVATE_MODULE_WARNING ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core CorePrivate Gui Widgets Sql PrintSupport)

qt_add_executable(GeSiER WIN32
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    coveragedashboard.cpp coveragedashboard.h
    traceabilitywidget.cpp traceabilitywidget.h
    n2matrixwidget.cpp n2matrixwidget.h
    interfaceservice.cpp interfaceservice.h
    interfacewidget.cpp interfacewidget.h
    changeservice.cpp changeservice.h
    changewidget.cpp changewidget.h
    producttreeservice.cpp producttreeservice.h
    producttreemodel.cpp producttreemodel.h
    producttreewidget.cpp producttreewidget.h
    requirementservice.cpp requirementservice.h
    requirementrelationservice.cpp requirementrelationservice.h
    documentservice.cpp documentservice.h
    docxexportservice.cpp docxexportservice.h
    documentwidget.cpp documentwidget.h
    requirementwidget.cpp requirementwidget.h
    applicabilityservice.cpp applicabilityservice.h
    applicabilitywidget.cpp applicabilitywidget.h
    verificationservice.cpp verificationservice.h
    verificationwidget.cpp verificationwidget.h
    checkablecombobox.cpp checkablecombobox.h
    xlsxreader.cpp xlsxreader.h
    requirementimportdialog.cpp requirementimportdialog.h
    importvalidationdialog.cpp importvalidationdialog.h
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
    Qt6::Core Qt6::CorePrivate Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::PrintSupport
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
        producttreeservice.cpp producttreeservice.h
        requirementservice.cpp requirementservice.h
        applicabilityservice.cpp applicabilityservice.h
        verificationservice.cpp verificationservice.h
        interfaceservice.cpp interfaceservice.h
        changeservice.cpp changeservice.h
        xlsxreader.cpp xlsxreader.h
        requirementrelationservice.cpp requirementrelationservice.h
        documentservice.cpp documentservice.h
        docxexportservice.cpp docxexportservice.h
        req_sqlmanager.cpp req_sqlmanager.h
        sqltreemodel.cpp sqltreemodel.h
        sqltreeitem.cpp sqltreeitem.h
        qressources.qrc
    )
    target_link_libraries(database_migrator_test PRIVATE Qt6::Core Qt6::CorePrivate Qt6::Widgets Qt6::Sql Qt6::Test)
    target_include_directories(database_migrator_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME database_migrator COMMAND database_migrator_test -txt)

    qt_add_executable(requirement_widget_test
        tests/requirement_widget_test.cpp
        requirementwidget.cpp requirementwidget.h
        checkablecombobox.cpp checkablecombobox.h
        requirementimportdialog.cpp requirementimportdialog.h
        importvalidationdialog.cpp importvalidationdialog.h
        xlsxreader.cpp xlsxreader.h
        requirementservice.cpp requirementservice.h
        requirementrelationservice.cpp requirementrelationservice.h
        documentservice.cpp documentservice.h
        producttreeservice.cpp producttreeservice.h
        databasemigrator.cpp databasemigrator.h
        req_sqlmanager.cpp req_sqlmanager.h
        qressources.qrc
    )
    target_link_libraries(requirement_widget_test PRIVATE Qt6::Core Qt6::CorePrivate Qt6::Widgets Qt6::Sql Qt6::Test)
    target_include_directories(requirement_widget_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME requirement_widget COMMAND requirement_widget_test -txt)
    set_tests_properties(requirement_widget PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    if(WIN32)
        get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(_qt_runtime_dir "${_qt_qmake}" DIRECTORY)
        get_filename_component(_compiler_runtime_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        string(REPLACE ";" "\\;" _test_path "$ENV{PATH}")
        set_tests_properties(database_migrator requirement_widget PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${_qt_runtime_dir}\\;${_compiler_runtime_dir}\\;${_test_path}")
    endif()

    qt_add_executable(xlsxreader_test
        tests/xlsxreader_test.cpp
        xlsxreader.cpp xlsxreader.h
    )
    target_link_libraries(xlsxreader_test PRIVATE Qt6::Core Qt6::CorePrivate Qt6::Test)
    target_include_directories(xlsxreader_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME xlsxreader COMMAND xlsxreader_test -txt)
    if(WIN32)
        set_tests_properties(xlsxreader PROPERTIES
            ENVIRONMENT "PATH=${_qt_runtime_dir}\\;${_compiler_runtime_dir}\\;${_test_path}")
    endif()
endif()

install(TARGETS GeSiER
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
