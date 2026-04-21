#include <gtest/gtest.h>
#include "core/SearchHistory.h"
#include <vector>
#include <string>

TEST(SearchHistoryTests, AddToEmptyHistory) {
    std::vector<std::string> history;
    AddToSearchHistory(history, "keyword");
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0], "keyword");
}

TEST(SearchHistoryTests, AddMovesExistingToFront) {
    std::vector<std::string> history = {"alpha", "beta", "gamma"};
    AddToSearchHistory(history, "beta");
    ASSERT_EQ(history.size(), 3u);
    EXPECT_EQ(history[0], "beta");
    EXPECT_EQ(history[1], "alpha");
    EXPECT_EQ(history[2], "gamma");
}

TEST(SearchHistoryTests, AddTrimsWhitespace) {
    std::vector<std::string> history;
    AddToSearchHistory(history, "  hello  ");
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0], "hello");
}

TEST(SearchHistoryTests, AddEmptyStringDoesNothing) {
    std::vector<std::string> history;
    AddToSearchHistory(history, "");
    AddToSearchHistory(history, "   ");
    EXPECT_TRUE(history.empty());
}

TEST(SearchHistoryTests, AddLimitsToTen) {
    std::vector<std::string> history;
    for (int i = 0; i < 15; ++i) {
        AddToSearchHistory(history, "item" + std::to_string(i));
    }
    ASSERT_EQ(history.size(), 10u);
    EXPECT_EQ(history[0], "item14");
    EXPECT_EQ(history[9], "item5");
}

TEST(SearchHistoryTests, RemoveExistingItem) {
    std::vector<std::string> history = {"alpha", "beta", "gamma"};
    RemoveFromSearchHistory(history, "beta");
    ASSERT_EQ(history.size(), 2u);
    EXPECT_EQ(history[0], "alpha");
    EXPECT_EQ(history[1], "gamma");
}

TEST(SearchHistoryTests, RemoveNonexistentDoesNothing) {
    std::vector<std::string> history = {"alpha", "beta"};
    RemoveFromSearchHistory(history, "missing");
    EXPECT_EQ(history.size(), 2u);
}

TEST(SearchHistoryTests, ClearRemovesAll) {
    std::vector<std::string> history = {"a", "b", "c"};
    ClearSearchHistory(history);
    EXPECT_TRUE(history.empty());
}

TEST(SearchHistoryTests, ClearOnEmptyIsSafe) {
    std::vector<std::string> history;
    ClearSearchHistory(history);
    EXPECT_TRUE(history.empty());
}
