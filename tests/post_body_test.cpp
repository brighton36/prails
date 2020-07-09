#include "gtest/gtest.h"

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
