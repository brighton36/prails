#pragma once
#include <optional>
#include <vector>
#include <map>

#include "exceptions.hpp"
#include "utilities.hpp"

namespace Controller {
  class PostBody {

    public:
      // TODO: I suppose this would work better if we stored members as variant<Hash,Array,Scalar>
      //       that would allow us to iterate over members via each...

      typedef std::vector<std::string> Array;
      typedef Controller::PostBody Hash;

      inline static const std::string MatchPairs = "([^=&]+)(?:=([^=&]*)[&]?)?";
      inline static const std::string MatchArrayKey = "^(.*)\\[\\]$";
      inline static const std::string MatchHashKey = "^([^\\]]+)\\[([^\\]]+)\\](.*)$";
      inline static const unsigned int MaxDepth = 32;
      inline static const std::string MatchUnsignedLong = "^[\\d]+$";
      inline static const std::string MatchDouble = 
        "^[\\-]?[\\d]+(?:|\\.[\\d]+)(?:|e[\\-]?[\\d]+)$";
      inline static const std::string MatchLongLongInt = "^[\\-]?[\\d]+$";
      inline static const std::string MatchInt = "^[\\-]?[\\d]+$";
      inline static const std::string MatchIso8601 = 
        "^[\\d]{4}\\-[\\d]{2}\\-[\\d]{2}T[\\d]{2}\\:[\\d]{2}\\:[\\d]{2}Z$";

      // NOTE: The depth exists mostly as a failsafe. It's conceivable that an 
      // attacker can cause us problems by recursing the input to a significant depth.
      explicit PostBody(unsigned int depth = 0) : depth(depth) {}
      explicit PostBody(const std::string &, unsigned int depth = 0);

      template <typename... Args>
      Array keys(std::string key, Args... args) {
        return (has_hash(key)) ? hashes[key].keys(args...) : Array();
      }
      Array keys(const std::string &);
      Array keys();

      bool has_key(const std::string &);
      bool has_scalar(const std::string &);
      bool has_hash(const std::string &);
      bool has_collection(const std::string &);
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

      template <typename... Args>
      void each(std::string key, Args... args) {
        if constexpr (sizeof...(Args) > 1) {
          if (has_hash(key)) hashes[key].each(args...);
        } else {
          if (has_collection(key)) 
            std::for_each(collections[key].cbegin(), collections[key].cend(), args...);
          else
            throw std::invalid_argument(fmt::format("\"{}\" not an array", key));
        }
      }

      template <typename T = std::string, typename... Args>
      std::optional<T> operator() (const std::string &key, Args... args) {
        if (hashes.count(key)) return hashes[key](args...);
        return std::nullopt;
      }

      template <typename T = std::string>
      std::optional<T> operator() (const std::string &key, int offset) {
        if (!has_collection(key)) return std::nullopt;
        try {
          return std::make_optional(collections[key].at(offset));
        } catch (const std::out_of_range& ) { return std::nullopt; }
      }

      template <typename T = std::string>
      std::optional<T> operator() (const std::string &key) {
        return (has_scalar(key)) ? operator[]<T>(key) : std::nullopt;
      }

      // This method only returns a scalar
      template <typename T = std::string>
      std::optional<T> operator[](const std::string &key) {
        if (!has_scalar(key)) return std::nullopt;

        std::string s = scalars[key];

        // An empty string isn't a nullopt. I think this is the best we can do,
        // even if it's an exceptional case. Because the frontend can always 
        // omit a key, and in doing so, convey a nullopt that way..
        if constexpr (std::is_same_v<T, std::string>)
          return s;

        // This means we're a non-string type, with an empty value
        if (s.empty()) return std::nullopt;

        T ret;

        if constexpr (std::is_same_v<T, unsigned long>) {
          if (!regex_match(s, std::regex(MatchUnsignedLong)))
            throw std::invalid_argument(fmt::format("\"{}\" not an unsigned long", key));
          ret = std::stoul(s);
        } else if constexpr (std::is_same_v<T, double>) {
          if (!regex_match(s, std::regex(MatchDouble)))
            throw std::invalid_argument(fmt::format("\"{}\" not a double", key));
          ret = std::stod(s);
        } else if constexpr (std::is_same_v<T, long long int>) {
          if (!regex_match(s, std::regex(MatchLongLongInt)))
            throw std::invalid_argument(fmt::format("\"{}\" not a long long int", key));
          ret = std::stoll(s);
        } else if constexpr (std::is_same_v<T, int>) {
          if (!regex_match(s, std::regex(MatchInt)))
            throw std::invalid_argument(fmt::format("\"{}\" not an int", key));
          ret = std::stoi(s);
        } else if constexpr (std::is_same_v<T, std::tm>) {
          if (!regex_match(s, std::regex(MatchIso8601)))
            throw std::invalid_argument(fmt::format("\"{}\" not a tm", key));
          ret = prails::utilities::iso8601_to_tm(s);
        } else 
          // Probably this should be a static_assert(false), but we're targetting
          // C++17 ...
          throw PostBodyException("Invalid typename requested of PostBody::operator[]");

        return std::make_optional<T>(ret);
      }

    private:
      unsigned int depth;
      const std::string urldecode(const std::string&);
      inline unsigned char char_from_hexchar (unsigned char);
      std::map<std::string, std::string> scalars;
      std::map<std::string, PostBody::Array> collections;
      std::map<std::string, PostBody::Hash> hashes;
  };
}
