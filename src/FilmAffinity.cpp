#include <iostream>
#include "FilmAffinity.hpp"

FilmAffinity::FilmAffinity() : Net () { }
FilmAffinity::~FilmAffinity(){ }

const std::string FilmAffinity::Url(UrlType type, std::string token) const {
  switch(type) {
    case Movie:
      return std::string("https://www.filmaffinity.com/es/film") + token + std::string(".html");
    case Search:
      return std::string("https://www.filmaffinity.com/es/search.php?stext=") + token;
    default:
      return std::string("");
  }
}

const std::string FilmAffinity::ParseTitle(std::string id) {
  Parse(Url(Movie, id));
  std::string title = parser.Root().Search(V, "id", "main-title")[0].Content();
  std::cout << title << std::endl;
  return title;
}
