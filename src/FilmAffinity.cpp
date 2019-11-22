#include <iostream>
#include <array>
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
      continue;
    } else if (field == "Duración") {
      std::cout << String::num(field_table[i].Next().Next().Children()[0].Content()) << std::endl;
      continue;
    } else if (field == "País") {
      std::cout << String::stripc(field_table[i].Next().Next().Children()[0].Children()[0].Attribute("title")) << std::endl;
      continue;
    } else if (field == "Dirección") {
      std::vector<Node> directors = field_table[i].Next().Next().Search(T, "a");
      std::vector<std::string> names;
      names.reserve(directors.size());
      for (int i = 0; i < directors.size(); i++) {
        names[i] = String::stripc(directors[i].Attribute("title"));
        std::cout << names[i];
        if (i < directors.size() - 1) std::cout << ", ";
      }
      std::cout << std::endl;
      continue;
    } else if (field == "Guion") {
      std::vector<Node> writers = field_table[i].Next().Next().Search(V, "class", "nb");
      std::vector<std::string> names;
      names.reserve(writers.size());
      for (int i = 0; i < writers.size(); i++) {
        names[i] = String::stripc(writers[i].Children()[0].Children()[0].Content());
        std::cout << names[i];
        if (i < writers.size() - 1) std::cout << ", ";
      }
      std::cout << std::endl;
      continue;
    }

    else {
      std::cout << String::stripc(field_table[i].Next().Next().Children()[0].Content()) << std::endl;
      continue;
    }
  }

  return title;
}

bool FilmAffinity::Test() { }
