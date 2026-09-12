cmake_minimum_required(VERSION 3.24)
project(PunPunIDE VERSION 0.5.2 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network Svg Qml)
find_package(Qt6 6.4 QUIET COMPONENTS Test)
find_package(LibArchive REQUIRED)

qt_add_executable(punpun-ide
    src/main.cpp
    src/MainWindow.cpp src/MainWindow.h
    src/AssistantPanel.cpp src/AssistantPanel.h
    src/SmartAnalyzer.cpp src/SmartAnalyzer.h
    src/PunPunLanguage.h
    src/CodeEditor.cpp src/CodeEditor.h
    src/SyntaxHighlighter.cpp src/SyntaxHighlighter.h
    src/Theme.cpp src/Theme.h
    src/IconUtils.cpp src/IconUtils.h
    src/FileIconProvider.cpp src/FileIconProvider.h
    src/Settings.cpp src/Settings.h
    src/TerminalWidget.cpp src/TerminalWidget.h
    src/LspClient.cpp src/LspClient.h
    src/ToolchainManager.cpp src/ToolchainManager.h
    src/ToolchainDialog.cpp src/ToolchainDialog.h
    src/PackageManagerDialog.cpp src/PackageManagerDialog.h
    src/SettingsDialog.cpp src/SettingsDialog.h
    src/ScriptHost.cpp src/ScriptHost.h
    c_api/punpun_plugin_api.c c_api/punpun_plugin_api.h
    resources/resources.qrc
)

target_include_directories(punpun-ide PRIVATE src c_api)
target_link_libraries(punpun-ide PRIVATE
    Qt6::Widgets Qt6::Network Qt6::Svg Qt6::Qml
    LibArchive::LibArchive
)

target_compile_definitions(punpun-ide PRIVATE
    PPIDE_VERSION="${PROJECT_VERSION}"
    QT_NO_KEYWORDS
)

if(MSVC)
  target_compile_options(punpun-ide PRIVATE /W4 /permissive-)
else()
  target_compile_options(punpun-ide PRIVATE -Wall -Wextra -Wpedantic)
endif()


add_executable(ppide-pp-bridge src/PpxBridgeMain.cpp)
target_compile_features(ppide-pp-bridge PRIVATE cxx_std_20)

install(TARGETS punpun-ide ppide-pp-bridge RUNTIME DESTINATION bin BUNDLE DESTINATION .)
install(FILES resources/branding/punpun-mark.svg DESTINATION share/punpun-ide)

# Desktop integration, so a Linux install shows up in the launcher and can be
# set as the handler for source files.
if(UNIX AND NOT APPLE)
  install(FILES punpun-ide.desktop DESTINATION share/applications)
  install(FILES resources/branding/punpun-mark.svg
          DESTINATION share/icons/hicolor/scalable/apps
          RENAME punpun-ide.svg)
  install(FILES packaging/punpun-ide.xml DESTINATION share/mime/packages)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(ppide-c-api-test tests/test_c_api.c c_api/punpun_plugin_api.c)
  target_include_directories(ppide-c-api-test PRIVATE c_api)
  add_test(NAME c_api COMMAND ppide-c-api-test)

  # Editor behaviour needs a real widget, so it runs under Qt Test offscreen.
  if(Qt6Test_FOUND)
    qt_add_executable(ppide-editor-test
      tests/test_editor.cpp
      src/CodeEditor.cpp src/CodeEditor.h
      src/SyntaxHighlighter.cpp src/SyntaxHighlighter.h
      src/ScriptHost.cpp src/ScriptHost.h
      c_api/punpun_plugin_api.c c_api/punpun_plugin_api.h
    )
    target_include_directories(ppide-editor-test PRIVATE src c_api)
    target_link_libraries(ppide-editor-test PRIVATE Qt6::Widgets Qt6::Test Qt6::Qml)
    target_compile_definitions(ppide-editor-test PRIVATE QT_NO_KEYWORDS)
    add_test(NAME editor COMMAND ppide-editor-test)
    set_tests_properties(editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(ppide-analyzer-test
      tests/test_analyzer.cpp
      src/SmartAnalyzer.cpp src/SmartAnalyzer.h
      src/PunPunLanguage.h
      src/CodeEditor.cpp src/CodeEditor.h
      src/SyntaxHighlighter.cpp src/SyntaxHighlighter.h
      src/ScriptHost.cpp src/ScriptHost.h
      c_api/punpun_plugin_api.c c_api/punpun_plugin_api.h
    )
    target_include_directories(ppide-analyzer-test PRIVATE src c_api)
    target_link_libraries(ppide-analyzer-test PRIVATE Qt6::Widgets Qt6::Test Qt6::Qml)
    target_compile_definitions(ppide-analyzer-test PRIVATE QT_NO_KEYWORDS)
    add_test(NAME analyzer COMMAND ppide-analyzer-test)
    set_tests_properties(analyzer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME ppx_bridge
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bridge.py $<TARGET_FILE:ppide-pp-bridge>)
    add_test(NAME source_audit
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/source_audit.py)
    # Compares the offline vocabulary against whatever toolchain is installed.
    # Skips cleanly when there is none, so it does not gate a toolchain-free build.
    add_test(NAME language_currency
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_language_currency.py)
    if(UNIX)
      add_test(NAME terminal_protocol
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_terminal_protocol.py)
    endif()
  endif()
endif()

set(CPACK_PACKAGE_NAME "PunPun-IDE")
set(CPACK_PACKAGE_VENDOR "PunPun")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fast native IDE for PunPun, C and C++")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "PunPun IDE")
set(CPACK_PACKAGE_EXECUTABLES "punpun-ide;PunPun IDE")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/pumpumlang/punpun-ide")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
if(WIN32)
  set(CPACK_GENERATOR "ZIP;NSIS")
  # A double-clickable installer: Start-menu and desktop shortcuts, and an
  # uninstaller, rather than a folder of loose files.
  set(CPACK_NSIS_PACKAGE_NAME "PunPun IDE")
  set(CPACK_NSIS_DISPLAY_NAME "PunPun IDE ${PROJECT_VERSION}")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\punpun-ide.exe")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MODIFY_PATH ON)
  set(CPACK_NSIS_CREATE_ICONS_EXTRA
      "CreateShortCut '$DESKTOP\\\\PunPun IDE.lnk' '$INSTDIR\\\\bin\\\\punpun-ide.exe'")
  set(CPACK_NSIS_DELETE_ICONS_EXTRA
      "Delete '$DESKTOP\\\\PunPun IDE.lnk'")
else()
  set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
