cmake_minimum_required(VERSION 3.24)

project(FragmentsPlayer
    VERSION 0.44.6
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia Test)
include(GNUInstallDirs)
include(CTest)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(fragments
    src/main.cpp
    src/export/ExportController.cpp
    src/export/ExportController.h
    src/playlist/Fragment.h
    src/playlist/PlaylistModel.cpp
    src/playlist/PlaylistModel.h
    src/playback/PlaybackBackend.cpp
    src/playback/PlaybackBackend.h
    src/playback/PlaybackController.cpp
    src/playback/PlaybackController.h
    src/playback/QtMediaPlaybackBackend.cpp
    src/playback/QtMediaPlaybackBackend.h
)

qt_add_qml_module(fragments
    URI Fragments
    VERSION 1.0
    QML_FILES
        src/ui/qml/Main.qml
)

qt_add_resources(fragments app_assets
    PREFIX /
    FILES
        assets/icons/io.github.ajunior.fragments.svg
        assets/fonts/MaterialSymbolsOutlined[FILL,GRAD,opsz,wght].ttf
)

target_include_directories(fragments
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(fragments
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Multimedia
)

if(BUILD_TESTING)
    qt_add_executable(playlist_model_tests
        tests/PlaylistModelTests.cpp
        src/playlist/Fragment.h
        src/playlist/PlaylistModel.cpp
        src/playlist/PlaylistModel.h
    )

    target_include_directories(playlist_model_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(playlist_model_tests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(NAME playlist_model_tests COMMAND playlist_model_tests)

    qt_add_executable(playback_controller_tests
        tests/PlaybackControllerTests.cpp
        src/playback/PlaybackBackend.cpp
        src/playback/PlaybackBackend.h
        src/playback/PlaybackController.cpp
        src/playback/PlaybackController.h
        src/playback/QtMediaPlaybackBackend.cpp
        src/playback/QtMediaPlaybackBackend.h
        src/playlist/Fragment.h
        src/playlist/PlaylistModel.cpp
        src/playlist/PlaylistModel.h
    )

    target_include_directories(playback_controller_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(playback_controller_tests
        PRIVATE
            Qt6::Core
            Qt6::Test
            Qt6::Multimedia
    )

    add_test(NAME playback_controller_tests COMMAND playback_controller_tests)
endif()

install(TARGETS fragments
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES packaging/io.github.ajunior.fragments.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES packaging/io.github.ajunior.fragments.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

install(FILES assets/icons/io.github.ajunior.fragments.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

set(CPACK_PACKAGE_NAME "Fragments")
set(CPACK_PACKAGE_VENDOR "Adjamilton Junior")
set(CPACK_PACKAGE_CONTACT "https://github.com/ajunior/fragments/issues")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Scripted playback for media fragments")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ajunior/fragments")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "fragments-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "TGZ")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
include(CPack)
