#pragma once

#include "Downloader.hpp"
#include "Parser.hpp"

enum Website { NONE, IMDB, FILMAFFINITY };
enum UrlType { MOVIE, SEARCH };

/**
 * Class for downloading and parsing movies and movies searches.
 *
 * Each supported website is implemented as a namespace. It is extensible.
 * This class is a singleton, only one instance can be created.
 */
class Net
{
private:
  Downloader downloader;
  Parser parser;
public:
  Net();
  ~Net();

  static Website Host(const std::string& url);
  
  /**
   * Download HTML file using libCURL and parse it using MyHTML
   */
  void Parse(const std::string& url);

  /**
   * Singleton: Remove possibility of duplicating object.
   */
  Net(const Net&) = delete;            // Remove copy constructor
  Net& operator=(const Net&) = delete; // Remove asignment operator
};
