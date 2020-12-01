#include "prails_gtest.hpp"

using namespace std;

TEST(Server, ExtToMime) {
  ASSERT_TRUE(Server::ExtToMime("3gpp").has_value());
  ASSERT_EQ("video/3gpp", *Server::ExtToMime("3gpp"));

  ASSERT_TRUE(Server::ExtToMime("zip").has_value());
  ASSERT_EQ("application/zip", *Server::ExtToMime("zip"));

  ASSERT_TRUE(Server::ExtToMime("ZIP").has_value());
  ASSERT_EQ("application/zip", *Server::ExtToMime("ZIP"));

  ASSERT_TRUE(Server::ExtToMime("mp3").has_value());
  ASSERT_EQ("audio/mpeg", *Server::ExtToMime("mp3"));

  ASSERT_TRUE(Server::ExtToMime("Mp3").has_value());
  ASSERT_EQ("audio/mpeg", *Server::ExtToMime("Mp3"));

  ASSERT_TRUE(Server::ExtToMime("html").has_value());
  ASSERT_EQ("text/html", *Server::ExtToMime("html"));
}
