#include <string> 
#include <vector> 
#include <regex> 
#include <string_view>

namespace prails::utilities {
  bool path_is_readable(const std::string &);
  std::string remove_trailing_slash(const std::string &);
  bool starts_with(std::string, std::string);
  std::string read_file(std::string path);
  std::string join(std::vector<std::string>, std::string);
  std::vector<std::string> split(const std::string &, const std::string &);
  std::vector<std::string> split(const std::string_view *, const std::string &);
  bool has_any(std::vector<std::string> haystack, std::vector<std::string> needles);
  std::regex regex_from_string(std::string);
  std::string replace_all(const std::string &, const std::string &,const std::string &);
  std::string tm_to_json(std::tm tm_time);
  std::pair<int,std::string> capture_system(const std::string &);
}
