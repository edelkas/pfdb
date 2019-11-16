#include <iostream>
#include "Net.hpp"
#include "FilmAffinity.hpp"

Net::Net() : downloader(true), parser() {
  //downloader = new Downloader(true);
  //parser = new Parser;
}

Net::~Net() {

}

void Net::Parse(Website web, const std::string& url) {
  parser.Parse(downloader.Get(url).c_str());
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
