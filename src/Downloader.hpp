#pragma once

#include <string>
#include <iostream>
#include <curl/curl.h>

#define SETOPT(x,e) x;if(res!=CURLE_OK){std::cout<<e<<std::endl<<error<<std::endl;}

/**
 * Simple class to provide HTTP downloading functionalities to PFDB
 * using libCURL.
 *
 * This class is a singleton, only one instance can be created.
 */
class Downloader
{
private:
  CURL *curl;                  // CURL handle
  CURLcode res;                // Operation return code
  char error[CURL_ERROR_SIZE]; // Buffer to store CURL error message
  std::string buffer;          // Result of the transfer
  bool safe;                   // Perform safety checks
public:
  Downloader(bool safety);
  ~Downloader();
  const std::string& Get(std::string url);
  inline const std::string& Retrieve() const { return buffer; }
  inline void Print() const { std::cout << buffer << std::endl; }

  /**
   * Singleton: Remove possibility of duplicating object.
   */
  Downloader(const Downloader&) = delete;            // Remove copy constructor
  Downloader& operator=(const Downloader&) = delete; // Remove asignment operator
private:
  int InitCurl();
  static size_t Write(char *data, size_t size, size_t nmemb, std::string *result);
};
