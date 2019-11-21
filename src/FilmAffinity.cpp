#include <iostream>
#include <vector>
#include "Net.hpp"
#include "FilmAffinity.hpp"
#include "Tools.hpp"

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
  // TODO: Change this ugly mess into something better, maybe using enums and a switch
  // TODO: Search for itemprops
  for (int i = 0; i < field_table.size(); i++) {
    std::string field = field_table[i].Children()[0].Content();
    std::cout << field << ": ";
    if (field == "Título original") {
      std::cout << String::stripc(field_table[i].Next().Next().Children()[0].Content()) << std::endl;
      continue;
    } else if (field == "Año") {
      std::cout << String::num(field_table[i].Next().Next().Children()[0].Content()) << std::endl;
    } else if (field == "Duración") {
      std::cout << String::num(field_table[i].Next().Next().Children()[0].Content()) << std::endl;
    } else if (field == "País") {
      std::cout << String::stripc(field_table[i].Next().Next().Children()[0].Children()[0].Attribute("title")) << std::endl;
    }

    else {
      std::cout << String::stripc(field_table[i].Next().Next().Children()[0].Content()) << std::endl;
    }
  }

  return title;
}

bool FilmAffinity::Test() { }
