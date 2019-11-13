#pragma once

#include "Downloader.hpp"
#include "Parser.hpp"

/**
 * Type of URL: Movie, Search, ...
 */
enum UrlType { Movie, Search };

/**
 * Class for downloading and parsing movies and movies searches.
 *
 * Each supported website is implemented as a child class. It is extensible.
 * This class is a singleton, only one instance can be created.
 */
class Net
{
private:
  Downloader downloader;
protected:
  Parser parser;
public:
  Net();
  ~Net();

  /**
   * Download HTML file using libCURL and parse it using MyHTML
   */
  void Parse(std::string url);

  /**
   * Singleton: Remove possibility of duplicating object.
   */
  Net(const Net&) = delete;            // Remove copy constructor
  Net& operator=(const Net&) = delete; // Remove asignment operator
};