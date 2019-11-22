#pragma once

#include <string>
#include "Net.hpp"

/**
 * Parse movies and perform searches from the web FilmAffinity.
 */
namespace FilmAffinity
{

  /**
   * This function uses a fixed film to check that everything is being parsed
   * correctly, in prevention of the website changing.
   */
  bool Test();

  const std::string Url(UrlType type, const std::string& token);
  void ParseField(const Node& node, std::string(*func)(const Node&));
  const std::string Parse(const Node& root);

};
