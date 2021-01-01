#pragma once
#include <optional>
#include <vector>
#include <map>

namespace Controller {
  class PostBody {
    public:
      typedef std::vector<std::string> Array;
      typedef Controller::PostBody Hash;

      inline static const std::string MatchPairs = "([^=&]+)(?:=([^=&]*)[&]?)?";
      inline static const std::string MatchArrayKey = "^(.*)\\[\\]$";
      inline static const std::string MatchHashKey = "^([^\\]]+)\\[([^\\]]+)\\](.*)$";
      inline static const unsigned int MaxDepth = 32;

      // NOTE: The depth exists mostly as a failsafe. It's conceivable that an 
      // attacker can cause us problems by recursing the input to a significant depth.
      explicit PostBody(unsigned int depth = 0) : depth(depth) {}
      explicit PostBody(const std::string &, unsigned int depth = 0);
      PostBody::Array keys();
      void set(const std::string &, const std::string &);

      template <typename... Args>
      std::optional<unsigned int> size(std::string key, Args... args) {
        return (hashes.count(key)) ? hashes[key].size(args...) : std::nullopt;
      }
      std::optional<unsigned int> size(const std::string &);
      std::optional<unsigned int> size(); 

      template <typename... Args>
      std::optional<PostBody> postbody(std::string key, Args... args) {
        return (hashes.count(key)) ? hashes[key].postbody(args...) : std::nullopt;
      }
      std::optional<PostBody> postbody(const std::string &key) {
        return (hashes.count(key)) ? std::make_optional(hashes[key]) : std::nullopt;
      }

      // NOTE: We're really only supporting collections here for now. If needed,
      //       I suppose we could implement an each_pair for hashes...
      template <typename... Args>
      void each(std::string key, Args... args) {
        if constexpr (sizeof...(Args) > 1) {
          if (hashes.count(key)) hashes[key].each(args...);
        } else {
          if (collections.count(key)) 
            std::for_each(collections[key].cbegin(), collections[key].cend(), args...);
        }
      }

      template <typename... Args>
      std::optional<std::string> operator() (std::string key, Args... args) {
        if (hashes.count(key)) return hashes[key](args...);
        return std::nullopt;
      }
      std::optional<std::string> operator() (std::string key, int offset);
      std::optional<std::string> operator() (const std::string &);
      std::optional<std::string> operator[] (const std::string &);

    private:
      unsigned int depth;
      const std::string urldecode(const std::string&);
      inline unsigned char char_from_hexchar (unsigned char);
      std::map<std::string, std::string> scalars;
      std::map<std::string, PostBody::Array> collections;
      std::map<std::string, PostBody::Hash> hashes;
  };
}
