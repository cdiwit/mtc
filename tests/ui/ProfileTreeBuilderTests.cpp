#include <algorithm>
#include <vector>

#include "core/Types.h"
#include "ui/ProfileTreeBuilder.h"

#if defined(MTC_HAS_GTEST) && MTC_HAS_GTEST
#include <gtest/gtest.h>

TEST(ProfileTreeBuilderTests, MatchesNameDescriptionAndDirectoryCaseInsensitively) {
    Profile profile{"1", "Prod API", "Release config", "D:/Work/Proj/A", "", "", TerminalType::Cmd, {}, {}, "", ""};

    EXPECT_TRUE(MatchesSearch(profile, "prod"));
    EXPECT_TRUE(MatchesSearch(profile, "release"));
    EXPECT_TRUE(MatchesSearch(profile, "d:/work/proj"));
    EXPECT_FALSE(MatchesSearch(profile, "staging"));
}

TEST(ProfileTreeBuilderTests, SplitsWindowsAndUnixPathsIntoSegments) {
    EXPECT_EQ(SplitWorkingDirectory("D:/Work/Proj/A"), (std::vector<std::string>{"D:", "Work", "Proj", "A"}));
    EXPECT_EQ(SplitWorkingDirectory("/home/user/app"), (std::vector<std::string>{"/", "home", "user", "app"}));
}

TEST(ProfileTreeBuilderTests, GroupsProfilesIntoDirectoryTreeAndUnassignedBucket) {
    Profile a{"1", "Dev", "", "D:/Work/Proj/A", "", "", TerminalType::Cmd, {}, {}, "", ""};
    Profile b{"2", "Prod", "", "D:/Work/Proj/A", "", "", TerminalType::Cmd, {}, {}, "", ""};
    Profile c{"3", "NoDir", "", "", "", "", TerminalType::Cmd, {}, {}, "", ""};

    std::vector<const Profile*> profiles{&a, &b, &c};

    auto tree = BuildProfileTree(profiles);

    ASSERT_EQ(tree.children.size(), 2u);
    EXPECT_EQ(tree.children[0].label, "D:");
    EXPECT_EQ(tree.children[1].label, "未设置工作目录");

    const auto find_child_by_label = [](const ProfileTreeNode& parent, const std::string& label) -> const ProfileTreeNode* {
        auto it = std::find_if(parent.children.begin(), parent.children.end(), [&](const ProfileTreeNode& child) {
            return child.label == label;
        });
        return it == parent.children.end() ? nullptr : &(*it);
    };

    const auto has_profile = [](const ProfileTreeNode& node, const Profile* profile) {
        return std::find(node.profiles.begin(), node.profiles.end(), profile) != node.profiles.end();
    };

    const auto* driveNode = find_child_by_label(tree, "D:");
    ASSERT_NE(driveNode, nullptr);

    const auto* workNode = find_child_by_label(*driveNode, "Work");
    ASSERT_NE(workNode, nullptr);

    const auto* projNode = find_child_by_label(*workNode, "Proj");
    ASSERT_NE(projNode, nullptr);

    const auto* finalDirectoryNode = find_child_by_label(*projNode, "A");
    ASSERT_NE(finalDirectoryNode, nullptr);
    EXPECT_TRUE(has_profile(*finalDirectoryNode, &a));
    EXPECT_TRUE(has_profile(*finalDirectoryNode, &b));
    EXPECT_EQ(finalDirectoryNode->profiles.size(), 2u);

    const auto* unassignedNode = find_child_by_label(tree, "未设置工作目录");
    ASSERT_NE(unassignedNode, nullptr);
    EXPECT_TRUE(has_profile(*unassignedNode, &c));
    EXPECT_EQ(unassignedNode->profiles.size(), 1u);
}
#else
int mtc_profile_tree_builder_compile_probe() {
    return 0;
}
#endif
