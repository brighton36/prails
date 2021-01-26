#include "utilities.hpp"

#include "gtest/gtest.h"

using namespace std;
using namespace prails::utilities;

TEST(utilities_test, starts_with) {
  ASSERT_TRUE(true  == starts_with("/public/html/test_file.txt", "/public/html" ));
  ASSERT_TRUE(false == starts_with("/public/", "/public/html/"));
  ASSERT_TRUE(false == starts_with("/etc/password", "/public/html/"));
}

TEST(utilities_test, join) {
  ASSERT_TRUE( join({"John", "Chris", "Nancy"}, ", ") == "John, Chris, Nancy" );
  ASSERT_TRUE( join({"John", "Chris"}, ", ") == "John, Chris" );
  ASSERT_TRUE( join({"John"}, ", ") == "John" );
}

TEST(utilities_test, split) {
  // string:
  ASSERT_EQ( split("John, Chris, Nancy", ", "), 
    vector<string>({"John", "Chris", "Nancy"}));
  ASSERT_EQ( split("John, Chris", ", "), vector<string>({"John", "Chris"}));
  ASSERT_EQ( split("John", ", "), vector<string>({"John"}));

  constexpr std::string_view cardinal_directions[] { "North,South,East,West" };
  constexpr std::string_view updown[] { "North,South" };
  constexpr std::string_view down[] { "South" };

  // string_view:
  ASSERT_EQ( split(cardinal_directions, ","), 
    vector<string>({"North", "South", "East", "West"}));
  ASSERT_EQ( split(updown, ","), vector<string>({"North", "South"}));
  ASSERT_EQ( split(down, ","), vector<string>({"South"}));
}

TEST(utilities_test, regex_from_string) {
  regex anything = regex_from_string("//");

  ASSERT_TRUE( regex_match("", anything) );
  ASSERT_FALSE( regex_match("literally anything", anything) );

  regex insensitive_john = regex_from_string("/^JOHN$/i");
  ASSERT_TRUE( regex_match("John", insensitive_john) );
  ASSERT_TRUE( regex_match("john", insensitive_john) );

  regex sensitive_john = regex_from_string("/^John$/");
  ASSERT_TRUE( regex_match("John", sensitive_john) );
  ASSERT_FALSE( regex_match("john", sensitive_john) );

  regex undecorated_regex = regex_from_string("Party");
  ASSERT_TRUE( regex_match("Party", undecorated_regex) );
  ASSERT_FALSE( regex_match("party", undecorated_regex) );

  // This was giving us problems in the model at one point...
  ASSERT_TRUE( regex_match("test@account.com", regex_from_string("/.+@.+/")) );
}

TEST(utilities_test, replace_all) {
  ASSERT_EQ( replace_all("All cats are good cats", "cats", "dogs"), "All dogs are good dogs" );
  ASSERT_EQ( replace_all("All cats are good cats", "are", "resent"), "All cats resent good cats" );
}

TEST(utilities_test, json_to_tm) {
  tm epoch1 = iso8601_to_tm("2020-11-05T20:54:14Z");
  EXPECT_EQ(epoch1.tm_mon, 10); // NOTE: Month 0 is January
  EXPECT_EQ(epoch1.tm_mday, 5);
  EXPECT_EQ(epoch1.tm_year, 120); // NOTE: the number of full calendar years since 1900
  EXPECT_EQ(epoch1.tm_hour, 20);
  EXPECT_EQ(epoch1.tm_min, 54);
  EXPECT_EQ(epoch1.tm_sec, 14);
  EXPECT_EQ(epoch1.tm_gmtoff, 0);
  EXPECT_EQ(1604609654, timegm(&epoch1));

  tm epoch2 = iso8601_to_tm("2020-05-01T01:02:03-0500");
  EXPECT_EQ(epoch2.tm_mon, 4); // NOTE: Month 0 is January
  EXPECT_EQ(epoch2.tm_mday, 1);
  EXPECT_EQ(epoch2.tm_year, 120); // NOTE: the number of full calendar years since 1900
  EXPECT_EQ(epoch2.tm_hour, 1);
  EXPECT_EQ(epoch2.tm_min, 2);
  EXPECT_EQ(epoch2.tm_sec, 3);
  EXPECT_EQ(epoch2.tm_gmtoff, (-5 * 3600));
}
