#include "Tools.hpp"

void String::lstrip(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
}

void String::rstrip(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
}

void String::strip(std::string &s) {
  rstrip(s);
  lstrip(s);
}

std::string String::stripc(std::string s) {
  strip(s);
  return s;
}

int String::num(const std::string& s) {
  int i;
  sscanf(s.c_str(), "%d", &i);
  return i;
}
