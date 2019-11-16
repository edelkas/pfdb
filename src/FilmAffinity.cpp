#include <iostream>
#include <vector>
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
  std::cout << "Título: " << title << std::endl;

  std::vector<Node> field_table = root.Search(V, "class", "movie-info")[0].Search(T, "dt");

  //std::vector<Node> fields;
  //fields.reserve(field_table.size());
  for (int i = 0; i < field_table.size(); i++) {
    std::cout << field_table[i].Children()[0].Content() << ": ";
    std::cout << field_table[i].Next().Next().Children()[0].Content() << std::endl;
  }

  return title;
}

bool FilmAffinity::Test() { }
