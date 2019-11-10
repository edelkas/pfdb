#include <iostream>
#include "Download.hpp"

Download::Download(bool safety) {
  curl_global_init(CURL_GLOBAL_DEFAULT);    // Initialize CURL global functionality
  curl = curl_easy_init();                  // Initialize CURL easy interface

  if (!safety) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Skip certificate verification
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // Skip hostname verification
  }
}

Download::~Download() {
  curl_global_cleanup();                    // Perform CURL global cleaup
}

int Download::Get(const char* url){
  curl = curl_easy_init();                  // Initialize CURL easy interface
  if (!curl) {
    std::cout << "Error: CURL didn't initialize properly." << std::endl;
    return 1;
  }
  curl_easy_setopt(curl, CURLOPT_URL, url); // Set URL
  res = curl_easy_perform(curl);            // Perform GET method
  if (res != CURLE_OK) {
    std::cout << "Error: CURL GET request wasn't successful." << std::endl;
    std::cout << curl_easy_strerror(res) << std::endl;
    return 1;
  }
  curl_easy_cleanup(curl);                  // Perform cleanup
  return 0;
}
