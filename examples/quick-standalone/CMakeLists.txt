cmake_minimum_required(VERSION 3.19...3.21)

project(QMapLibreExampleQuickStandalone VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 REQUIRED)
qt_standard_project_setup()

if(QT_KNOWN_POLICY_QTP0002)
    qt_policy(SET QTP0002 NEW)
endif()

find_package(QMapLibre COMPONENTS Core Quick REQUIRED)

qt_add_executable(
    QMapLibreExampleQuickStandalone
    main.cpp
)

qt_add_qml_module(
    QMapLibreExampleQuickStandalone
    URI Example
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES
        main.qml
)

set_target_properties(
    QMapLibreExampleQuickStandalone
    PROPERTIES
        WIN32_EXECUTABLE ON
        MACOSX_BUNDLE ON
        QT_WASM_PTHREAD_POOL_SIZE 16
)

target_link_libraries(
    QMapLibreExampleQuickStandalone
    PRIVATE
        Qt::Quick
        Qt::QuickControls2
        QMapLibre::Core
)

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    target_link_options(
        QMapLibreExampleQuickStandalone
        PRIVATE
            -sTEXTDECODER=0
    )
endif()

qmaplibre_quick_setup_plugins(QMapLibreExampleQuickStandalone)

install(
    TARGETS QMapLibreExampleQuickStandalone
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_qml_app_script(
    TARGET QMapLibreExampleQuickStandalone
    OUTPUT_SCRIPT deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT ${deploy_script})

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    install(
        IMPORTED_RUNTIME_ARTIFACTS QMapLibre::Core
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    include(FetchContent)
    FetchContent_Declare(
        android_openssl
        DOWNLOAD_EXTRACT_TIMESTAMP true
        URL https://github.com/KDAB/android_openssl/archive/refs/heads/master.zip
    )
    FetchContent_MakeAvailable(android_openssl)
    include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)

    add_android_openssl_libraries(QMapLibreExampleQuickStandalone)

    # Bundle Qt's WebP imageformats plugin, which the Qt CI deployment omits by
    # default. Without it QImage/QImageReader cannot decode WebP, so WebP raster
    # tiles fail (e.g. the Open Waters elevation/terrain source).
    if(DEFINED ENV{QT_ROOT_DIR})
        set(_qmaplibre_webp_plugin
            "$ENV{QT_ROOT_DIR}/plugins/imageformats/libplugins_imageformats_qwebp_${ANDROID_ABI}.so")
        if(EXISTS "${_qmaplibre_webp_plugin}")
            set_property(TARGET QMapLibreExampleQuickStandalone APPEND PROPERTY
                QT_ANDROID_EXTRA_LIBS "${_qmaplibre_webp_plugin}")
        endif()
    endif()
endif()

# Packaging
set(CPACK_GENERATOR TBZ2)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CPACK_SYSTEM_NAME "macOS")
else()
    set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}")
endif()

include(CPack)
