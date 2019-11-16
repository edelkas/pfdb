#include <iostream>
#include "Net.hpp"
#include "FilmAffinity.hpp"

const std::string FilmAffinity::Url(UrlType type, const std::string& token) {
  switch(type) {
    case MOVIE:
      return std::string("https://www.filmaffinity.com/es/film") + token + std::string(".html");
    case SEARCH:
      return std::string("https://www.filmaffinity.com/es/search.php?stext=") + token;
    default:
      return std::string("");
  }
}

const std::string FilmAffinity::Parse(const Node& root) {
  std::string title = root.Search(V, "id", "main-title")[0].Children()[1].Children()[0].Content();
  std::cout << title << std::endl;
  return title;
}

bool FilmAffinity::Test() { }
