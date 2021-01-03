#include "gtest/gtest.h"

#include <limits>
#include <pistache/http.h>
#include <pistache/stream.h>
#include <pistache/router.h>

#include "server.hpp"

using namespace std;

TEST(post_body_test, uri_decode) {
  Controller::PostBody post;

  post = Controller::PostBody("name=Person&type=horse&color=cat");
  ASSERT_EQ(post.size(), 3);
  ASSERT_EQ(post["name"], "Person");
  ASSERT_EQ(post["type"], "horse");
  ASSERT_EQ(post["color"], "cat");

  post = Controller::PostBody( 
    "name=New+Task+%26+testing+out+%22yes%22+in+quotes&description=huh+%3D+huh%0ANew+line");

  ASSERT_EQ(post.size(), 2);
  ASSERT_EQ(post["name"], "New Task & testing out \"yes\" in quotes");
  ASSERT_EQ(post["description"], "huh = huh\nNew line");

  post = Controller::PostBody("name=");
  ASSERT_EQ(post.size(), 1);
  ASSERT_TRUE( post["name"].value().empty() );

  post = Controller::PostBody("name=value");
  ASSERT_EQ(post.size(), 1);
  ASSERT_EQ(*post("name"), "value");
  
  post = Controller::PostBody("name=&type");
  ASSERT_EQ(post.size(), 2);
  ASSERT_TRUE( post["name"].value().empty() );
  ASSERT_TRUE( post["type"].value().empty() );

  post = Controller::PostBody("name=&type=value");
  ASSERT_EQ(post.size(), 2);
  ASSERT_TRUE( post["name"].value().empty() );
  ASSERT_EQ(post["type"], "value");

  post = Controller::PostBody("&type");
  ASSERT_EQ(post.size(), 1);
  ASSERT_TRUE( post["type"].value().empty() );

  post = Controller::PostBody("type&");
  ASSERT_EQ(post.size(), 1);
  ASSERT_TRUE( post["type"].value().empty() );

  post = Controller::PostBody("&&x");
  ASSERT_EQ(post.size(), 1);
  ASSERT_TRUE( post["x"].value().empty() );

  post = Controller::PostBody("x&&");
  ASSERT_EQ(post.size(), 1);
  ASSERT_TRUE( post["x"].value().empty() );

  post = Controller::PostBody("x==");
  ASSERT_EQ(post.size(), 1);
  ASSERT_TRUE( post["x"].value().empty() );

  post = Controller::PostBody("&&");
  ASSERT_EQ(post.size(), 0);

  post = Controller::PostBody("==x");
  ASSERT_EQ(post.size(), 1);

  post = Controller::PostBody("=y");
  ASSERT_EQ(post.size(), 1);

  post = Controller::PostBody("path[=test");
  ASSERT_EQ(post.size(), 1);

  post = Controller::PostBody("path=test[]");
  ASSERT_EQ(post.size(), 1);

  post = Controller::PostBody("[]=y");
  ASSERT_EQ(*post.size(), 0);

  post = Controller::PostBody("");
  ASSERT_TRUE(post["parameter_that_doesnt_exist"] == nullopt);
}

TEST(customer_controller_test, uri_decode_array) {
  Controller::PostBody post;
  unsigned int i = 0;

  post = Controller::PostBody("ids%5B%5D=1361&ids%5B%5D=1376&request%5Bactive%5D=0");
  ASSERT_EQ(*post.size(), 2);
  ASSERT_EQ(*post.size("ids"), 2);
  ASSERT_EQ(*post("ids", 0), "1361");
  ASSERT_EQ(*post("ids", 1), "1376");
  ASSERT_EQ(*post("request", "active"), "0");

  post = Controller::PostBody(
    "ids%5B%5D=2&ids%5B%5D=4&ids%5B%5D=6&ids%5B%5D=8&ids%5B%5D=10&ids%5B%5D=12&"
    "ids%5B%5D=14&ids%5B%5D=16&request%5Bactive%5D=0");
  ASSERT_EQ(*post.size(), 2);

  ASSERT_EQ(*post.size("ids"), 8);
  ASSERT_EQ(*post("ids", 0), "2");
  ASSERT_EQ(*post("ids", 1), "4");
  ASSERT_EQ(*post("ids", 2), "6");
  ASSERT_EQ(*post("ids", 3), "8");
  ASSERT_EQ(*post("ids", 4), "10");
  ASSERT_EQ(*post("ids", 5), "12");
  ASSERT_EQ(*post("ids", 6), "14");
  ASSERT_EQ(*post("ids", 7), "16");

  // Let's make sure the each iterator works:
  i = 0;
  post.each("ids", [&i](const std::string &v) { i += 1; ASSERT_EQ(i*2, stoi(v)); });

  ASSERT_EQ(*post("request", "active"), "0");

  // Just making sure this doesn't bork:
  post = Controller::PostBody();
  ASSERT_EQ(*post.size(), 0);
  ASSERT_EQ(post.size("ids"), nullopt);
  ASSERT_EQ(post("ids", 0), nullopt);
  ASSERT_EQ(post("request", "active"), nullopt);
  
  post = Controller::PostBody(
    "color=green&ids%5B%5D=1361&ids%5B%5D=1376&cat%5Bfur%5D=brown&cat%5Btoes%5D"
    "=11&cat%5Beyes%5D=black&cat%5Bparents%5D%5B%5D=charlie&cat%5Bp"
    "arents%5D%5B%5D=diana&cat%5Bthoughts%5D%5Bmice%5D=yum&cat%5Bthough"
    "ts%5D%5Bwater%5D=questionable&cat%5Bthoughts%5D%5Bowner%5D=disdain");

  ASSERT_EQ(*post.size(), 3);

  ASSERT_EQ(*post("color"), "green");

  ASSERT_EQ(*post.size("ids"), 2);
  ASSERT_EQ(*post("ids", 0), "1361");
  ASSERT_EQ(*post("ids", 1), "1376");

  ASSERT_EQ(*post.size("cat"), 5);
  ASSERT_EQ(*post("cat", "fur"), "brown");
  ASSERT_EQ((*post.postbody("cat"))["fur"], "brown");
  ASSERT_EQ(*post("cat", "toes"), "11");
  ASSERT_EQ((*post.postbody("cat"))["toes"], "11");
  ASSERT_EQ(*post("cat", "eyes"), "black");
  ASSERT_EQ((*post.postbody("cat"))["eyes"], "black");

  ASSERT_EQ(*post.size("cat", "thoughts"), 3);
  ASSERT_EQ(*post("cat", "thoughts", "mice"), "yum");
  ASSERT_EQ((*post.postbody("cat", "thoughts"))["mice"], "yum");
  ASSERT_EQ(*post("cat", "thoughts", "water"), "questionable");
  ASSERT_EQ((*post.postbody("cat", "thoughts"))["water"], "questionable");
  ASSERT_EQ(*post("cat", "thoughts", "owner"), "disdain");
  ASSERT_EQ((*post.postbody("cat", "thoughts"))["owner"], "disdain");

  ASSERT_EQ(*post.size("cat", "parents"), 2);
  ASSERT_EQ(*post("cat", "parents", 0), "charlie");
  ASSERT_EQ(*post("cat", "parents", 1), "diana");

  // Let's make sure the each iterator works:
  i = 0;
  post.each("cat", "parents", [&i](const std::string &v) { 
    if (i == 0) {ASSERT_EQ(v, "charlie");}
    else if (i == 1) {ASSERT_EQ(v, "diana");}
    i += 1; 
  });

  // Now let's try to break the parser. None of these values should exist:
  ASSERT_EQ(post.size("cat", "parents", "doesntexist"), nullopt);
  ASSERT_EQ(post.size("doesntexist"), nullopt);
  ASSERT_EQ(post.postbody("doesntexist"), nullopt);
  ASSERT_EQ(post.postbody("cat", "parents", "doesntexist"), nullopt);
  ASSERT_EQ(post("cat", "parents", "doesntexist"), nullopt);
  ASSERT_EQ(post("cat", "doesntexist"), nullopt);
  ASSERT_EQ(post("doesntexist"), nullopt);
  ASSERT_EQ(post["doesntexist"], nullopt);
  ASSERT_EQ(post("ids", 2), nullopt);
  ASSERT_EQ(post("ids", -2), nullopt);

  // What happens if we use the same key to set both a scalar and an array:
  post = Controller::PostBody("test=working&test%5B%5D=notworking");
  ASSERT_EQ(*post.size(), 1);
  ASSERT_EQ(*post("test"), "working");

  // What happens if we use the same key to set both a scalar and a hash:
  post = Controller::PostBody("test=working&test%5Bsubkey%5D=notworking");
  ASSERT_EQ(*post.size(), 1);
  ASSERT_EQ(*post("test"), "working");

  // What happens if we use the same key to set both an array and a hash:
  post = Controller::PostBody("test%5B%5D=working&test%5Bsubkey%5D=notworking");
  ASSERT_EQ(*post.size(), 1);
  ASSERT_EQ(*post("test", 0), "working");

  // What happens if we use the same key to set a scalar, an array, and a hash:
  post = Controller::PostBody("test=working&test%5B%5D=notworking&test%5Bsubkey%5D=notworking");
  ASSERT_EQ(*post.size(), 1);
  ASSERT_EQ(*post("test"), "working");

  // First set stores. Additional sets are ignored:
  post = Controller::PostBody("test=first&test=second&test=third");
  ASSERT_EQ(*post.size(), 1);
  ASSERT_EQ(*post("test"), "first");
}

TEST(post_body_test, scalar_typecasting) {
  // I'm not sure exactly how compiler-specific this is... but, seems like its
  // worth testing.... Though some of the results seem weird here...
  Controller::PostBody post(
    string("numeric=456")+
    "&string=abc"+
    "&time=2020-11-05T20%3A54%3A14Z"+ // 2020-11-05T20:54:14Z

    "&llong_max="+to_string(numeric_limits<long long int>::max())+
    "&i_max="+to_string(numeric_limits<int>::max())+
    "&ulong_max="+to_string(numeric_limits<unsigned long>::max())+
    "&dbl_max="+to_string(numeric_limits<double>::max())+

    "&llong_min="+to_string(numeric_limits<long long int>::min())+
    "&i_min="+to_string(numeric_limits<int>::min())+
    "&ulong_min="+to_string(numeric_limits<unsigned long>::min())+
    // For some reason, the double string conversions are weird in C++'s libraries...
    // so, I just kinda chose this.
    "&dbl_min=9.22337203685478e-18" 
    );

  // Basic numeric Typecasting...
  optional<string> numeric_as_string = post.operator[]<string>("numeric");
  optional<int> numeric_as_int = post.operator[]<int>("numeric");
  optional<unsigned long> numeric_as_ulong = post.operator[]<unsigned long>("numeric");
  optional<double> numeric_as_double = post.operator[]<double>("numeric");
  optional<long long int> numeric_as_lli = post.operator[]<long long int>("numeric");

  EXPECT_THROW(post.operator[]<tm>("numeric"), std::invalid_argument);
  EXPECT_EQ(*numeric_as_string, "456");
  EXPECT_EQ(*numeric_as_int, 456);
  EXPECT_EQ(*numeric_as_ulong, 456);
  EXPECT_EQ(*numeric_as_double, 456);
  EXPECT_EQ(*numeric_as_lli, 456);

  // <int>::max() casting into ...
  optional<string> i_max_as_string = post.operator[]<string>("i_max");
  optional<int> i_max_as_int = post.operator[]<int>("i_max");
  optional<unsigned long> i_max_as_ulong = post.operator[]<unsigned long>("i_max");
  optional<double> i_max_as_double = post.operator[]<double>("i_max");
  optional<long long int> i_max_as_lli = post.operator[]<long long int>("i_max");

  EXPECT_THROW(post.operator[]<tm>("i_max"), std::invalid_argument);
  EXPECT_EQ(*i_max_as_string, to_string(numeric_limits<int>::max()));
  EXPECT_EQ(*i_max_as_int, numeric_limits<int>::max()); 
  EXPECT_EQ(*i_max_as_ulong, numeric_limits<int>::max());
  EXPECT_EQ(*i_max_as_double, numeric_limits<int>::max());
  EXPECT_EQ(*i_max_as_lli, numeric_limits<int>::max());

  // <int>::min() casting into ...
  optional<string> i_min_as_string = post.operator[]<string>("i_min");
  optional<int> i_min_as_int = post.operator[]<int>("i_min");
  optional<double> i_min_as_double = post.operator[]<double>("i_min");
  optional<long long int> i_min_as_lli = post.operator[]<long long int>("i_min");

  EXPECT_THROW(post.operator[]<tm>("i_min"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<unsigned long>("i_min"), std::invalid_argument);
  EXPECT_EQ(*i_min_as_string, to_string(numeric_limits<int>::min()));
  EXPECT_EQ(*i_min_as_int, numeric_limits<int>::min()); 
  EXPECT_EQ(*i_min_as_double, numeric_limits<int>::min());
  EXPECT_EQ(*i_min_as_lli, numeric_limits<int>::min());

  // <unsigned long>::max() casting into ...
  optional<string> ulong_max_as_string = post.operator[]<string>("ulong_max");
  optional<unsigned long> ulong_max_as_ulong = post.operator[]<unsigned long>("ulong_max");
  optional<double> ulong_max_as_double = post.operator[]<double>("ulong_max");

  EXPECT_THROW(post.operator[]<tm>("ulong_max"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<int>("ulong_max"), std::out_of_range);
  EXPECT_THROW(post.operator[]<long long int>("ulong_max"), std::out_of_range);
  EXPECT_EQ(*ulong_max_as_string, to_string(numeric_limits<unsigned long>::max()));
  EXPECT_EQ(*ulong_max_as_ulong, numeric_limits<unsigned long>::max());
  EXPECT_EQ(*ulong_max_as_double, numeric_limits<unsigned long>::max());

  // <unsigned long>::min() casting into ...
  optional<string> ulong_min_as_string = post.operator[]<string>("ulong_min");
  optional<unsigned long> ulong_min_as_ulong = post.operator[]<unsigned long>("ulong_min");
  optional<double> ulong_min_as_double = post.operator[]<double>("ulong_min");
  optional<int> ulong_min_as_int = post.operator[]<int>("ulong_min");
  optional<long long int> ulong_min_as_lli = post.operator[]<long long int>("ulong_min");

  EXPECT_THROW(post.operator[]<tm>("ulong_min"), std::invalid_argument);
  EXPECT_EQ(*ulong_min_as_string, to_string(numeric_limits<unsigned long>::min()));
  EXPECT_EQ(*ulong_min_as_ulong, numeric_limits<unsigned long>::min());
  EXPECT_EQ(*ulong_min_as_double, numeric_limits<unsigned long>::min());
  EXPECT_EQ(*ulong_min_as_int, numeric_limits<unsigned long>::min());
  EXPECT_EQ(*ulong_min_as_lli, numeric_limits<unsigned long>::min());

  // <long long int>::max() casting into ...
  optional<string> llong_max_as_string = post.operator[]<string>("llong_max");
  optional<unsigned long> llong_max_as_ulong = post.operator[]<unsigned long>("llong_max");
  optional<double> llong_max_as_double = post.operator[]<double>("llong_max");
  optional<long long int> llong_max_as_lli = post.operator[]<long long int>("llong_max");

  EXPECT_THROW(post.operator[]<tm>("llong_max"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<int>("llong_max"), std::out_of_range);
  EXPECT_EQ(*llong_max_as_string, to_string(numeric_limits<long long int>::max()));
  EXPECT_EQ(*llong_max_as_ulong, numeric_limits<long long int>::max());
  EXPECT_EQ(*llong_max_as_double, numeric_limits<long long int>::max());
  EXPECT_EQ(*llong_max_as_lli, numeric_limits<long long int>::max());

  // <long long int>::min() casting into ...
  optional<string> llong_min_as_string = post.operator[]<string>("llong_min");
  optional<double> llong_min_as_double = post.operator[]<double>("llong_min");
  optional<long long int> llong_min_as_lli = post.operator[]<long long int>("llong_min");

  EXPECT_THROW(post.operator[]<tm>("llong_min"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<int>("llong_min"), std::out_of_range);
  EXPECT_THROW(post.operator[]<unsigned long>("llong_min"), std::invalid_argument);
  EXPECT_EQ(*llong_min_as_string, to_string(numeric_limits<long long int>::min()));
  EXPECT_EQ(*llong_min_as_double, numeric_limits<long long int>::min());
  EXPECT_EQ(*llong_min_as_lli, numeric_limits<long long int>::min());

  // <double>::max() casting into ...
  optional<string> dbl_max_as_string = post.operator[]<string>("dbl_max");
  optional<double> dbl_max_as_double = post.operator[]<double>("dbl_max");
  
  EXPECT_THROW(post.operator[]<tm>("dbl_max"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<int>("dbl_max"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<unsigned long>("dbl_max"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<long long int>("dbl_max"), std::invalid_argument);
  EXPECT_EQ(*dbl_max_as_string, to_string(numeric_limits<double>::max()));
  EXPECT_EQ(*dbl_max_as_double, numeric_limits<double>::max());
  
  // <double>::min() casting into ...
  optional<string> dbl_min_as_string = post.operator[]<string>("dbl_min");
  optional<double> dbl_min_as_double = post.operator[]<double>("dbl_min");
  
  EXPECT_THROW(post.operator[]<tm>("dbl_min"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<int>("dbl_min"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<unsigned long>("dbl_min"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<long long int>("dbl_min"), std::invalid_argument);
  EXPECT_EQ(*dbl_min_as_string, "9.22337203685478e-18");
  EXPECT_EQ(*dbl_min_as_double, 9.22337203685478e-18);

  // tm casting into ...
  optional<string> time_as_string = post.operator[]<string>("time");
  optional<tm> time_as_tm = post.operator[]<tm>("time");

  EXPECT_THROW(post.operator[]<int>("time"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<unsigned long>("time"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<double>("time"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<long long int>("time"), std::invalid_argument);
  EXPECT_EQ(prails::utilities::tm_to_iso8601(*time_as_tm), "2020-11-05T20:54:14Z");
  EXPECT_EQ(*time_as_string, "2020-11-05T20:54:14Z");
  
  // Typecasting characters to number types...
  optional<string> string_as_string = post.operator[]<string>("string");

  EXPECT_THROW(post.operator[]<tm>("string"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<int>("string"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<unsigned long>("string"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<double>("string"), std::invalid_argument);
  EXPECT_THROW(post.operator[]<long long int>("string"), std::invalid_argument);
  EXPECT_EQ(*string_as_string, "abc");
}

TEST(post_body_test, double_typecasting) {
  Controller::PostBody post("formA=-3.34&formB=3.34&formC=78&formD=2.5e-10"
    "&formE=2.5e10&formF=2e10");

  optional<double> formA = post.operator[]<double>("formA");
  EXPECT_EQ(*formA, -3.34);
  optional<double> formB = post.operator[]<double>("formB");
  EXPECT_EQ(*formB, 3.34);

  optional<double> formC = post.operator[]<double>("formC");
  EXPECT_EQ(*formC, 78);

  optional<double> formD = post.operator[]<double>("formD");
  EXPECT_EQ(*formD, 2.5e-10);

  optional<double> formE = post.operator[]<double>("formE");
  EXPECT_EQ(*formE, 2.5e10);

  optional<double> formF = post.operator[]<double>("formF");
  EXPECT_EQ(*formF, 2e10);
}

TEST(post_body_test, empty_typecasting) {
  Controller::PostBody post("&time=&llong=&i=&ulong=&dbl=");

  EXPECT_EQ(post.operator[]<tm>("time"), nullopt);
  EXPECT_EQ(post.operator[]<long long int>("llong"), nullopt);
  EXPECT_EQ(post.operator[]<int>("i"), nullopt);
  EXPECT_EQ(post.operator[]<unsigned long>("ulong"), nullopt);
  EXPECT_EQ(post.operator[]<double>("dbl"), nullopt);
}
