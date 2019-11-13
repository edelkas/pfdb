#include "Net.hpp"

Net::Net() : downloader(true), parser() {
  //downloader = new Downloader(true);
  //parser = new Parser;
}

Net::~Net() {

}

void Net::Parse(std::string url) {
  parser.Parse(downloader.Get(url).c_str());
}
