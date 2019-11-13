#pragma once

#include <string>
#include "Net.hpp"

/**
 * Parse movies and perform searches from the web FilmAffinity.
 */
class FilmAffinity : public Net
{
public:
  FilmAffinity();
  ~FilmAffinity();

  const std::string Url(UrlType type, std::string token) const;
  const std::string ParseTitle(std::string id);
};
