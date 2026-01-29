#pragma once
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace PathUtils {
    // 获取可执行文件所在目录
    fs::path GetExecutableDir();
    
    // 获取数据目录
    fs::path GetDataDir();
    
    // 确保目录存在
    bool EnsureDirectory(const fs::path& path);
    
    // 路径是否存在
    bool Exists(const fs::path& path);
    
    // 规范化路径
    fs::path Normalize(const fs::path& path);
}
