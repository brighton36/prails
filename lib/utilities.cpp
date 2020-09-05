#include "utilities.hpp"
#include <filesystem>
#include <fstream>
#include <numeric>

using namespace std;

namespace prails::utilities {

bool path_is_readable(const string &path) {
  filesystem::path p(path);

  error_code ec;
  auto perms = filesystem::status(p, ec).permissions();

  return ( (ec.value() == 0) && (
    (perms & filesystem::perms::owner_read) != filesystem::perms::none &&
    (perms & filesystem::perms::group_read) != filesystem::perms::none &&
    (perms & filesystem::perms::others_read) != filesystem::perms::none ) );
}

string remove_trailing_slash(const string &path) {
  smatch matches;
  if (regex_search(path, matches, regex("^(.+)[\\\\/]$"))) return matches[1];
  return path;
}

bool starts_with(string haystack, string needle) {
  return (haystack.compare(0, needle.size(), needle) == 0);
}

string read_file(string path) {
  ifstream f(path);
  string ret;

  f.seekg(0, ios::end);   
  ret.reserve(f.tellg());
  f.seekg(0, ios::beg);

  ret.assign((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());

  return ret;
}

string join(vector<string> strings, string delim) {
  return accumulate(strings.begin(), strings.end(), string(), 
    [&](const string& a, const string& b) -> string { 
        return a + (a.length() > 0 ? delim : "") + b; 
    });
}

bool has_any(vector<string> haystack, vector<string> needles) {
  return (find_if(haystack.begin(), haystack.end(), 
    [&needles] (const string &s) { 
      return (
        find_if(needles.begin(), needles.end(), [&s] (const string &n) { return n == s; }
      ) != needles.end());
    } 
  ) != haystack.end());
}

regex regex_from_string(string re) {
  smatch re_parts;
  if (regex_search(re, re_parts, regex("^\\/(.*)\\/([i]?)$")))
    return (re_parts[2] == "i") ? 
      regex((string)re_parts[1], regex_constants::icase) : regex((string) re_parts[1]);
  
  return regex(re);
}

/// Returns a version of 'str' where every occurrence of
/// 'find' is substituted by 'replace'.
/// - Inspired by James Kanze.
/// - http://stackoverflow.com/questions/20406744/
std::string replace_all(const std::string & haystack, const std::string & needle,
  const std::string & replace) {
  string result;
  size_t needle_len = needle.size();
  size_t pos,from=0;
  while ( string::npos != ( pos=haystack.find(needle,from) ) ) {
    result.append( haystack, from, pos-from );
    result.append( replace );
    from = pos + needle_len;
  }
  result.append( haystack, from , string::npos );
  return result;
}

std::string tm_to_json(std::tm tm_time) {
  char buffer [80];
  long int t = timegm(&tm_time);
  strftime(buffer,80,"%Y-%m-%dT%H:%M:%S.0%z",std::gmtime(&t));
  return std::string(buffer);
}

}
