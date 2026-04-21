#include "SearchHistory.h"
#include <algorithm>

namespace {
std::string TrimKeyword(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}
}  // namespace

void AddToSearchHistory(std::vector<std::string>& history, const std::string& keyword) {
    std::string trimmed = TrimKeyword(keyword);
    if (trimmed.empty()) {
        return;
    }

    auto it = std::find(history.begin(), history.end(), trimmed);
    if (it != history.end()) {
        history.erase(it);
    }
    history.insert(history.begin(), trimmed);

    if (history.size() > 10) {
        history.resize(10);
    }
}

void RemoveFromSearchHistory(std::vector<std::string>& history, const std::string& keyword) {
    auto it = std::find(history.begin(), history.end(), keyword);
    if (it != history.end()) {
        history.erase(it);
    }
}

void ClearSearchHistory(std::vector<std::string>& history) {
    history.clear();
}
