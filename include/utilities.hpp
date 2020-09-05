#include <string> 
#include <vector> 
#include <regex> 

namespace prails::utilities {
  bool path_is_readable(const std::string &);
  std::string remove_trailing_slash(const std::string &);
  bool starts_with(std::string, std::string);
  std::string read_file(std::string path);
  std::string join(std::vector<std::string>, std::string);
  bool has_any(std::vector<std::string> haystack, std::vector<std::string> needles);
  std::regex regex_from_string(std::string);
  std::string replace_all(const std::string &, const std::string &,const std::string &);
  std::string tm_to_json(std::tm tm_time);
}
