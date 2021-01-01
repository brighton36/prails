#include <regex>
#include "post_body.hpp"

using namespace std;
using namespace Controller;

inline unsigned char PostBody::char_from_hexchar ( unsigned char ch ) {
  if (ch <= '9' && ch >= '0')
    ch -= '0';
  else if (ch <= 'f' && ch >= 'a')
    ch -= 'a' - 10;
  else if (ch <= 'F' && ch >= 'A')
    ch -= 'A' - 10;
  else 
    ch = 0;

  return ch;
}

const string PostBody::urldecode ( const string& str) {
  string result;
  string::size_type i;
  for (i = 0; i < str.size(); ++i) {
    if (str[i] == '+')
      result += ' ';
    else if (str[i] == '%' && str.size() > i+2) {
      const unsigned char ch1 = char_from_hexchar(str[i+1]);
      const unsigned char ch2 = char_from_hexchar(str[i+2]);
      const unsigned char ch = (ch1 << 4) | ch2;
      result += ch;
      i += 2;
    } else
      result += str[i];
  }
  return result;
}

PostBody::PostBody(const string &encoded, unsigned int depth) : depth(depth) {
  const regex parameter_pairs(MatchPairs); 
  smatch res;
  
  string::const_iterator search_begin(encoded.cbegin());
  while (regex_search(search_begin, encoded.cend(), res, parameter_pairs)) {
    search_begin = res.suffix().first;
    set(urldecode(res[1]), urldecode(res[2]));
  }
}

void PostBody::set(const string &key, const string &value) {
  smatch matches;
  if (regex_search(key, matches, regex(MatchHashKey))) {
    // Hash key parsed:
    if ((matches[1].length() > 0) && 
      (scalars.count(matches[1]) == 0) && (collections.count(matches[1]) == 0) &&
      (depth < MaxDepth)
    ) {
      if (hashes.count(matches[1]) == 0) hashes[matches[1]] = PostBody(depth+1);
      hashes[matches[1]].set(string(matches[2])+string(matches[3]), value); 
    }
  } else if (regex_search(key, matches, regex(MatchArrayKey))) {
    // Collections key parsed:
    if ((matches[1].length() > 0) && 
      (scalars.count(matches[1]) == 0) && (hashes.count(matches[1]) == 0)
    )
      collections[matches[1]].push_back(value);
  } else {
    // Scalar key parsed:
    if ((collections.count(key) == 0) && (hashes.count(key) == 0) && scalars.count(key) == 0)
      scalars[key] = value;
  }
}

optional<string> PostBody::operator[](const string &key) {
  return (scalars.count(key)) ? make_optional(scalars[key]) : nullopt;        
}

optional<string> PostBody::operator() (string key, int offset) {
  if (collections.count(key) == 0) return nullopt;
  try {
    return make_optional(collections[key].at(offset));
  } catch (const out_of_range& ) { return nullopt; }
}

optional<string> PostBody::operator() (const string &key) {
  return (scalars.count(key)) ? make_optional(scalars[key]) : nullopt;        
}

optional<unsigned int> PostBody::size(const string &key) {
  if (collections.count(key)) return make_optional(collections[key].size());
  else if (hashes.count(key)) return hashes[key].size();
  return nullopt;
}

optional<unsigned int> PostBody::size() {
  return scalars.size()+collections.size()+hashes.size();
}

PostBody::Array PostBody::keys() {
  PostBody::Array ret;

  for_each(begin(scalars), end(scalars), [&ret](const auto &p) {
    ret.push_back(p.first); });
  for_each(begin(hashes), end(hashes), [&ret](const auto &p) {
    ret.push_back(p.first); });
  for_each(begin(collections), end(collections), [&ret](const auto &p) {
    ret.push_back(p.first); });

  return ret;
}
