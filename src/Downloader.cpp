#include <iostream>
#include "Downloader.hpp"

Downloader::Downloader(bool safety) : safe(safety), error(""), buffer(""), res(CURLE_OK) {
  curl_global_init(CURL_GLOBAL_DEFAULT);    // Initialize CURL global functionality
  Downloader::InitCurl();
}

Downloader::~Downloader() {
  curl_global_cleanup();                    // Perform CURL global cleaup
}

int Downloader::InitCurl(){
  /* Try to initialize CURL easy interface */
  curl = curl_easy_init();
  if (!curl) {
    std::cout << "Error: CURL didn't initialize properly." << std::endl << error << std::endl;
    return 1;
  }

  /* Try to set CURL error buffer */
  SETOPT(curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error), "Error: CURL failed to set error buffer.");

  /* Try to set CURL redirect option */
  SETOPT(curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L), "Error: CURL failed to set redirect option.");

  /* Try to set CURL writer function */
  SETOPT(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Write), "Error: CURL failed to set writer function.");

  /* Try to set CURL write data */
  SETOPT(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer), "Error: CURL failed to set write data.");

  if (!safe) {
    /* Try to skip certificate verification */
    SETOPT(curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L), "Error: CURL failed to skip certificate verification.");

    /* Try to skip hostname verification */
    SETOPT(curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L), "Error: CURL failed to skip hostname verification.");
  }

  return 0;
}

const std::string& Downloader::Get(std::string url){
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); // Set URL
  res = curl_easy_perform(curl);                    // Perform GET method
  if (res != CURLE_OK) {
    std::cout << "Error: CURL GET request wasn't successful." << std::endl;
    std::cout << curl_easy_strerror(res) << std::endl;
    buffer = "";
  }
  curl_easy_cleanup(curl);                          // Perform cleanup
  return buffer;
}

/**
 * CURL internal callback method to store result of transfer.
 *
 * @param New chunk of data
 * @param Size of chunk of data in bytes
 * @param Number of chunks of data
 * @param Cumulative source where data is being stored
 */
size_t Downloader::Write(char *data, size_t size, size_t nmemb, std::string *result){
  if (result == NULL) return 0;
  result->append(data, size * nmemb);
  return size * nmemb;
}
