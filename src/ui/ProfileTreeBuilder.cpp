#include "ui/ProfileTreeBuilder.h"

#include <algorithm>
#include <cctype>

namespace {
constexpr const char* kUnassignedWorkingDirectoryLabel = "未设置工作目录";

std::string TrimText(const std::string& text) {
    const auto first = std::find_if(text.begin(), text.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });

    if (first == text.end()) {
        return "";
    }

    const auto last = std::find_if(text.rbegin(), text.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base();

    return std::string(first, last);
}

ProfileTreeNode* FindChildByLabel(ProfileTreeNode& parent, const std::string& label) {
    auto it = std::find_if(parent.children.begin(), parent.children.end(), [&](const ProfileTreeNode& child) {
        return child.label == label;
    });

    if (it == parent.children.end()) {
        return nullptr;
    }

    return &(*it);
}
}  // namespace

std::string NormalizeSearchText(const std::string& text) {
    std::string normalized = TrimText(text);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return normalized;
}

bool MatchesSearch(const Profile& profile, const std::string& normalizedSearch) {
    const std::string normalized = NormalizeSearchText(normalizedSearch);
    if (normalized.empty()) {
        return true;
    }

    const auto contains = [&](const std::string& value) {
        return NormalizeSearchText(value).find(normalized) != std::string::npos;
    };

    return contains(profile.name) || contains(profile.description)
        || contains(profile.workingDirectory) || contains(profile.linuxWorkingDirectory) || contains(profile.macWorkingDirectory);
}

std::vector<const Profile*> FilterProfiles(const std::vector<Profile>& profiles, const std::string& searchText) {
    const std::string normalizedSearch = NormalizeSearchText(searchText);

    std::vector<const Profile*> filtered;
    filtered.reserve(profiles.size());

    for (const auto& profile : profiles) {
        if (MatchesSearch(profile, normalizedSearch)) {
            filtered.push_back(&profile);
        }
    }

    return filtered;
}

std::vector<std::string> SplitWorkingDirectory(const std::string& workingDirectory) {
    const std::string trimmed = TrimText(workingDirectory);
    if (trimmed.empty()) {
        return {};
    }

    std::string normalizedPath = trimmed;
    std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

    std::vector<std::string> segments;

    std::size_t start = 0;
    if (!normalizedPath.empty() && normalizedPath[0] == '/') {
        segments.push_back("/");
        start = 1;
    }

    while (start < normalizedPath.size()) {
        while (start < normalizedPath.size() && normalizedPath[start] == '/') {
            ++start;
        }

        if (start >= normalizedPath.size()) {
            break;
        }

        const std::size_t end = normalizedPath.find('/', start);
        if (end == std::string::npos) {
            segments.push_back(normalizedPath.substr(start));
            break;
        }

        if (end > start) {
            segments.push_back(normalizedPath.substr(start, end - start));
        }
        start = end + 1;
    }

    return segments;
}

ProfileTreeNode BuildProfileTree(const std::vector<const Profile*>& profiles) {
    ProfileTreeNode root{"", "", {}, {}};

    for (const Profile* profile : profiles) {
        if (profile == nullptr) {
            continue;
        }

        const auto segments = SplitWorkingDirectory(profile->GetWorkingDirectory());

        if (segments.empty()) {
            ProfileTreeNode* unassignedNode = FindChildByLabel(root, kUnassignedWorkingDirectoryLabel);
            if (unassignedNode == nullptr) {
                root.children.push_back(ProfileTreeNode{kUnassignedWorkingDirectoryLabel, "", {}, {}});
                unassignedNode = &root.children.back();
            }
            unassignedNode->profiles.push_back(profile);
            continue;
        }

        ProfileTreeNode* currentNode = &root;
        std::string fullPath;

        for (std::size_t i = 0; i < segments.size(); ++i) {
            const auto& segment = segments[i];

            if (i == 0) {
                fullPath = segment;
            } else if (fullPath == "/") {
                fullPath += segment;
            } else {
                fullPath += "/" + segment;
            }

            ProfileTreeNode* nextNode = FindChildByLabel(*currentNode, segment);
            if (nextNode == nullptr) {
                currentNode->children.push_back(ProfileTreeNode{segment, fullPath, {}, {}});
                nextNode = &currentNode->children.back();
            }

            currentNode = nextNode;
        }

        currentNode->profiles.push_back(profile);
    }

    return root;
}
