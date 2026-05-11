#pragma once

#include <string>
#include <vector>

#include "core/Types.h"

std::string NormalizeSearchText(const std::string& text);
bool MatchesSearch(const Profile& profile, const std::string& normalizedSearch);
std::vector<const Profile*> FilterProfiles(const std::vector<Profile>& profiles, const std::string& searchText);
std::vector<std::string> SplitWorkingDirectory(const std::string& workingDirectory);
ProfileTreeNode BuildProfileTree(const std::vector<const Profile*>& profiles);
