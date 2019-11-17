#include <iostream>
#include "Net.hpp"
#include "FilmAffinity.hpp"

Net::Net() : downloader(true), parser() {
  //downloader = new Downloader(true);
  //parser = new Parser;
}

Net::~Net() {

}

Website Net::Host(const std::string& url) {
  if (url.find("imdb") != std::string::npos) {
    return IMDB;
  } else if (url.find("filmaffinity") != std::string::npos) {
    return FILMAFFINITY;
  } else {
    return NONE;
  }
}

void Net::Parse(const std::string& url) {
  parser.Parse(downloader.Get(url).c_str());
  Website web = Host(url);
  switch(web) {
    case FILMAFFINITY:
      FilmAffinity::Parse(parser.Root());
      break;
    case IMDB:

      break;
    default:
      std::cout << "Warning: Website not supported." << std::endl;
  }
}
