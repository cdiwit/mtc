cmake_minimum_required(VERSION 3.15)
project(MTC VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 wxWidgets
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# 查找 nlohmann_json
find_package(nlohmann_json 3.2.0 REQUIRED)

# 源文件
set(SOURCES
    src/main.cpp
    src/core/ConfigManager.cpp
    src/core/TerminalLauncher.cpp
    src/ui/MainFrame.cpp
    src/ui/ProfileDialog.cpp
    src/ui/EnvVarPanel.cpp
    src/utils/PathUtils.cpp
)

# 头文件目录
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)

# Windows 资源文件
if(WIN32)
    list(APPEND SOURCES resources/mtc.rc)
    add_definitions(-DUNICODE -D_UNICODE)
    set(CMAKE_WIN32_EXECUTABLE ON)
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

# MSVC UTF-8 编译选项
if(MSVC)
    add_compile_options(/utf-8)
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

# Windows 特定链接
if(WIN32)
    target_link_libraries(mtc PRIVATE shlwapi shell32)
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
else()
    add_custom_command(TARGET mtc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:mtc>/data
    )
endif()
