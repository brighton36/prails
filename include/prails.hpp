#include <map>

#include "config_parser.hpp"

namespace prails {
  typedef std::function<unsigned int(
    ConfigParser&, std::shared_ptr<spdlog::logger>, const std::vector<std::string> &)
    > ModeFunction;
  typedef std::function<void(
    ConfigParser&, std::shared_ptr<spdlog::logger>)
    > AppInitFunction;
  int main(int argc, char *argv[], std::map<std::string, ModeFunction> = {}, 
    AppInitFunction = nullptr);
}
