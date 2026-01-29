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

# 创建可执行文件
add_executable(mtc ${SOURCES})

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

# 静态链接运行时（Windows MSVC）
if(MSVC)
    set_property(TARGET mtc PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# 复制数据目录到输出目录
add_custom_command(TARGET mtc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:mtc>/data
)
