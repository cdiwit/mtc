#pragma once
#include <string>
#include <vector>

void AddToSearchHistory(std::vector<std::string>& history, const std::string& keyword);
void RemoveFromSearchHistory(std::vector<std::string>& history, const std::string& keyword);
void ClearSearchHistory(std::vector<std::string>& history);
