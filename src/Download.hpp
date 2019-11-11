#pragma once

#include <string>
#include <curl/curl.h>

#define SETOPT(x,e) x;if(res!=CURLE_OK){std::cout<<e<<std::endl<<error<<std::endl;}

/**
 * Simple class to provide HTTP downloading functionalities to PFDB
 * using libCURL.
 *
 * This class is a singleton, only one instance can be created.
 */
class Download
{
private:
  CURL *curl;                  // CURL handle
  CURLcode res;                // Operation return code
  char error[CURL_ERROR_SIZE]; // Buffer to store CURL error message
  std::string buffer;          // Result of the transfer
  bool safe;                   // Perform safety checks
public:
  Download(bool safety);
  ~Download();
  int Get(const char* url);
  inline void Print() { std::cout << buffer << std::endl; }

  /**
   * Singleton: Remove possibility of duplicating object.
   */
  Download(const Download&) = delete;            // Remove copy constructor
  Download& operator=(const Download&) = delete; // Remove asignment operator
private:
  int InitCurl();
  static size_t Write(char *data, size_t size, size_t nmemb, std::string *result);
};
