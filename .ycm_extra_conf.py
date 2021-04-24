import os
import ycm_core

def DirectoryOfThisScript():
  return os.path.dirname( os.path.abspath( __file__ ) )

def Settings( **kwargs ):
  return {
    'flags': [
        "-std=c++17",
        "-I/usr/include/mysql",
        "-Iinclude",
        "-Ibuild/_deps/pistache-src/include",
        "-Ibuild/_deps/pistache-src/tests",
        "-Ibuild/_deps/spdlog-src/include" ],
    'include_paths_relative_to_dir': DirectoryOfThisScript()
  }
