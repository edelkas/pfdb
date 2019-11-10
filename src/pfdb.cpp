#include <iostream>
#include <cstdio>
#include <vector>
#include <string>

#include "Download.hpp"
#include "Film.hpp"

void parse_param(int argc, char* argv[], std::vector<std::string> &switches, std::vector<std::string> &parameters){
  for (int i = 0; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (i < argc - 1) {
        if (argv[i + 1][0] == '-') {
          switches.push_back(argv[i]);
        } else {
          parameters.push_back(argv[i]);
        }
      } else {
        switches.push_back(argv[i]);
      }
    }
  }
}

int main(int argc, char* argv[]){
  std::vector<std::string> switches;
  std::vector<std::string> parameters;
  parse_param(argc, argv, switches, parameters);
  std::cout << "Switches: " << switches.size() << ", Parameters: " << parameters.size() << std::endl;

  Download downloader(true);
  downloader.Get("https://geohot.com/");
  return 0;
}
