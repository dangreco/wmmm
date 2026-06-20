#include <gtest/gtest.h>

#include "add.hpp"

TEST(Add, Positive) { EXPECT_EQ(add(2, 3), 5); }

TEST(Add, Negative) { EXPECT_EQ(add(-2, -3), -5); }

TEST(Add, Zero) { EXPECT_EQ(add(0, 0), 0); }
