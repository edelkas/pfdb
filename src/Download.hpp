#pragma once

#include <curl/curl.h>

/**
 * Simple class to provide HTTP downloading functionalities to PFDB
 * using libCURL.
 *
 * This class is a singleton, only one instance can be created.
 */
class Download
{
private:
  CURL *curl;   // CURL handle
  CURLcode res; // Transfer return code
  bool safety;  // Perform safety checks
public:
  Download(bool safety);
  ~Download();
  int Get(const char* url);

  /**
   * Singleton: Remove possibility of duplicating object.
   */
  Download(const Download&) = delete;            // Remove copy constructor
  Download& operator=(const Download&) = delete; // Remove asignment operator
};
