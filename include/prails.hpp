#include "config_parser.hpp"

namespace prails {
  typedef std::function<unsigned int(
    ConfigParser&, std::shared_ptr<spdlog::logger>, std::vector<std::string>)
    > ModeFunction;
  int main(int argc, char *argv[], std::map<std::string, ModeFunction> = {});
}
