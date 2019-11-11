#include <iostream>
#include "Parser.hpp"

Parser::Parser() : buffer("") {
  /* Initialize MyHTML */
  parser = myhtml_create();
  myhtml_init(parser, MyHTML_OPTIONS_DEFAULT, 1, 0);

  /* Initialize tree */
  tree = myhtml_tree_create();
  myhtml_tree_init(tree, parser);
}

Parser::~Parser() {
  /* Release resources */
  myhtml_tree_destroy(tree);
  myhtml_destroy(parser);
}

void Parser::Parse(const char* data) {
  myhtml_parse(tree, MyENCODING_UTF_8, data, strlen(data));
}

void Parser::Print() const {
  mycore_string_raw_t str = {0};
  myhtml_serialization_tree_buffer(myhtml_tree_get_document(tree), &str);
  std::cout << str.data << std::endl;
  mycore_string_raw_destroy(&str, false);
}
