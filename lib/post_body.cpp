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
      (!has_scalar(matches[1])) && (!has_collection(matches[1])) &&
      (depth < MaxDepth)
    ) {
      if (!has_hash(matches[1])) hashes[matches[1]] = PostBody(depth+1);
      hashes[matches[1]].set(string(matches[2])+string(matches[3]), value); 
    }
  } else if (regex_search(key, matches, regex(MatchArrayKey))) {
    // Collections key parsed:
    if ((matches[1].length() > 0) && 
      (!has_scalar(matches[1])) && (!has_hash(matches[1]))
    )
      collections[matches[1]].push_back(value);
  } else {
    // Scalar key parsed:
    if (!has_key(key)) scalars[key] = value;
  }
}

optional<unsigned int> PostBody::size(const string &key) {
  if (has_collection(key)) return make_optional(collections[key].size());
  else if (has_hash(key)) return hashes[key].size();
  return nullopt;
}

optional<unsigned int> PostBody::size() {
  return scalars.size()+collections.size()+hashes.size();
}

PostBody::Array PostBody::keys(const string &key) {
  if (has_hash(key)) return hashes[key].keys();
  return {};
}

PostBody::Array PostBody::keys() {
  PostBody::Array ret;

  for_each( begin(scalars), end(scalars), 
    [&ret](const std::pair<std::string, std::string> &p) {
      ret.push_back(p.first); 
    });
  for_each( begin(hashes), end(hashes), 
    [&ret](const std::pair<std::string, PostBody::Hash> &p) {
      ret.push_back(p.first); 
    });
  for_each(begin(collections), end(collections), 
    [&ret](const std::pair<std::string, PostBody::Array> &p) {
      ret.push_back(p.first); 
    });

  return ret;
}

bool PostBody::has_key(const std::string &key) {
  // I'm not sure that this is actually useful...
  return (has_scalar(key) || has_hash(key) || has_collection(key));
}

bool PostBody::has_scalar(const std::string &key) {
  return scalars.count(key) > 0; 
}

bool PostBody::has_hash(const std::string &key) {
  return hashes.count(key) > 0; 
}

bool PostBody::has_collection(const std::string &key) {
  return collections.count(key) > 0; 
}
