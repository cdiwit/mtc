cmake_minimum_required(VERSION 3.15)
project(MTC VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# 头文件目录
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)

# MSVC UTF-8 编译选项
if(MSVC)
    add_compile_options(/utf-8)
endif()

option(MTC_BUILD_APP "Build wxWidgets application target" ON)

if(MTC_BUILD_APP)
    # 查找 wxWidgets（仅主程序需要）
    find_package(wxWidgets QUIET COMPONENTS core base)

    if(wxWidgets_FOUND)
        include(${wxWidgets_USE_FILE})

        # 查找 nlohmann_json
        find_package(nlohmann_json 3.2.0 REQUIRED)

        # ---- libssh2（远程 SFTP 文件浏览/下载）----
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LIBSSH2 QUIET IMPORTED_TARGET libssh2)
        endif()
        if(NOT LIBSSH2_FOUND AND NOT TARGET PkgConfig::LIBSSH2)
            find_path(LIBSSH2_INCLUDE_DIR NAMES libssh2.h)
            find_library(LIBSSH2_LIBRARY NAMES ssh2 libssh2)
            if(LIBSSH2_INCLUDE_DIR AND LIBSSH2_LIBRARY)
                set(LIBSSH2_FOUND TRUE)
            endif()
        endif()
        if(NOT LIBSSH2_FOUND AND NOT TARGET PkgConfig::LIBSSH2)
            message(FATAL_ERROR "libssh2 未找到。请安装：macOS `brew install libssh2` / Linux `apt install libssh2-1-dev` / Windows vcpkg `libssh2`")
        endif()

        # ---- libsecret（Linux 钥匙串；macOS/Windows 用系统框架）----
        if(UNIX AND NOT APPLE)
            if(PkgConfig_FOUND)
                pkg_check_modules(LIBSECRET QUIET IMPORTED_TARGET libsecret-1)
            endif()
            if(NOT LIBSECRET_FOUND AND NOT TARGET PkgConfig::LIBSECRET)
                find_path(LIBSECRET_INCLUDE_DIR NAMES libsecret/secret.h)
                find_library(LIBSECRET_LIBRARY NAMES secret-1 libsecret-1)
                if(LIBSECRET_INCLUDE_DIR AND LIBSECRET_LIBRARY)
                    set(LIBSECRET_FOUND TRUE)
                endif()
            endif()
            if(NOT LIBSECRET_FOUND AND NOT TARGET PkgConfig::LIBSECRET)
                message(WARNING "libsecret 未找到，Linux 上凭据将无法保存到钥匙串。请 `apt install libsecret-1-dev`")
            endif()
        endif()

        # 源文件
        set(SOURCES
            src/main.cpp
            src/core/ConfigManager.cpp
            src/core/SearchHistory.cpp
            src/core/TerminalLauncher.cpp
            src/core/SecretStore.cpp
            src/core/SshClient.cpp
            src/ui/MainFrame.cpp
            src/ui/ProfileDialog.cpp
            src/ui/EnvVarPanel.cpp
            src/ui/ProfileTreeBuilder.cpp
            src/ui/SshHostDialog.cpp
            src/ui/SshHostManagerDialog.cpp
            src/ui/CredentialDialog.cpp
            src/ui/CredentialManagerDialog.cpp
            src/ui/RemoteFileBrowserDialog.cpp
            src/utils/PathUtils.cpp
        )

        # Windows 资源文件
        if(WIN32)
            list(APPEND SOURCES resources/mtc.rc)
            add_definitions(-DUNICODE -D_UNICODE)
        endif()

        # macOS 配置
        if(APPLE)
            set(MACOSX_BUNDLE_ICON_FILE mtc.icns)
            set(APP_ICON_MACOS ${CMAKE_SOURCE_DIR}/resources/mtc.icns)
            set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
                MACOSX_PACKAGE_LOCATION "Resources"
            )
            list(APPEND SOURCES ${APP_ICON_MACOS})
        endif()

        # 创建可执行文件
        if(APPLE)
            add_executable(mtc MACOSX_BUNDLE ${SOURCES})
        else()
            add_executable(mtc ${SOURCES})
        endif()

        # 链接库
        target_link_libraries(mtc PRIVATE
            ${wxWidgets_LIBRARIES}
            nlohmann_json::nlohmann_json
        )

        # libssh2 链接
        if(TARGET PkgConfig::LIBSSH2)
            target_link_libraries(mtc PRIVATE PkgConfig::LIBSSH2)
        else()
            target_include_directories(mtc PRIVATE ${LIBSSH2_INCLUDE_DIR})
            target_link_libraries(mtc PRIVATE ${LIBSSH2_LIBRARY})
        endif()

        # libsecret 链接（Linux）
        if(UNIX AND NOT APPLE)
            if(TARGET PkgConfig::LIBSECRET)
                target_link_libraries(mtc PRIVATE PkgConfig::LIBSECRET)
            elseif(LIBSECRET_FOUND)
                target_include_directories(mtc PRIVATE ${LIBSECRET_INCLUDE_DIR})
                target_link_libraries(mtc PRIVATE ${LIBSECRET_LIBRARY})
            endif()
        endif()

        # macOS 钥匙串框架（Security / CoreFoundation）
        if(APPLE)
            find_library(SECRET_SECURITY_FRAMEWORK Security REQUIRED)
            find_library(SECRET_CFFRAMEWORK CoreFoundation REQUIRED)
            target_link_libraries(mtc PRIVATE
                ${SECRET_SECURITY_FRAMEWORK} ${SECRET_CFFRAMEWORK})
        endif()

        # Windows 特定链接（凭据管理器 + Winsock + shell）
        if(WIN32)
            target_link_libraries(mtc PRIVATE shlwapi shell32 advapi32 ws2_32)
            set_target_properties(mtc PROPERTIES WIN32_EXECUTABLE TRUE)
        endif()

        # 设置输出目录
        set_target_properties(mtc PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
        )

        # macOS Bundle 配置
        if(APPLE)
            set_target_properties(mtc PROPERTIES
                MACOSX_BUNDLE TRUE
                MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/resources/Info.plist
                MACOSX_BUNDLE_BUNDLE_NAME "MTC"
                MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
                MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
                MACOSX_BUNDLE_GUI_IDENTIFIER "com.mtc.terminal-manager"
            )
        endif()

        # 静态链接运行时（Windows MSVC）
        if(MSVC)
            set_property(TARGET mtc PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        endif()

        # 复制数据目录到输出目录
        if(APPLE)
            # macOS: 复制到 app bundle 的 Resources 目录
            add_custom_command(TARGET mtc POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_BUNDLE_CONTENT_DIR:mtc>/Resources/data
            )
        elseif(UNIX)
            # Linux: 复制数据目录和图标到输出目录
            add_custom_command(TARGET mtc POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:mtc>/data
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_SOURCE_DIR}/resources/mtc.png
                    $<TARGET_FILE_DIR:mtc>/mtc.png
            )
        else()
            add_custom_command(TARGET mtc POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:mtc>/data
            )
        endif()
    else()
        message(WARNING "wxWidgets not found, skipping mtc application target; test target can still be configured")
    endif()
endif()

include(FetchContent)

find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    find_package(GTest QUIET)
endif()

if(NOT GTest_FOUND)
    message(STATUS "GTest not found locally, fetching googletest with FetchContent")
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        URL
            https://codeload.github.com/google/googletest/zip/refs/tags/v1.15.2
            https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(mtc_tests
    tests/ui/ProfileTreeBuilderTests.cpp
    tests/core/SearchHistoryTests.cpp
    src/ui/ProfileTreeBuilder.cpp
    src/core/SearchHistory.cpp
)

target_include_directories(mtc_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(mtc_tests PRIVATE MTC_HAS_GTEST=1)
target_link_libraries(mtc_tests PRIVATE GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(mtc_tests)
