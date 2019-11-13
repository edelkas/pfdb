#include <iostream>
#include "Parser.hpp"

/* NODE CLASS */

Node::Node(myhtml_tree_node_t *node) : node(node) {
  //std::cout << "Constructed Node." << std::endl;
}

Node::~Node() {
  /* Release resources */
  myhtml_node_free(node);
}

Node::Node(const Node& n) {
  //std::cout << "Copied Node." << std::endl;
}

std::vector<Node> Node::Children() const {
  // TODO: Maybe counting children first and then reserving the required space
  // in the vector to avoid resizing would be beneficial?
  // However, the only member of Node is a pointer, so moving it around isn't
  // that big a deal.
  std::vector<Node> children;
  int count = 0;
  myhtml_tree_node_t *n = myhtml_node_child(node);
  while (n != NULL && count < 1000) { // TODO: Make hardcoded limit flexible
    children.emplace_back(n);
    n = myhtml_node_next(n);
    count++;
  }

  return children;
}

std::vector<Node> Node::Search(SearchType type, const char* search, const char* value) const {
  myhtml_collection_t *result;
  std::vector<Node> nodes;
  switch(type) {
    case T:
      result = myhtml_get_nodes_by_name_in_scope(myhtml_node_tree(node), NULL, node, search, strlen(search), NULL);
      break;
    case A:
      result = myhtml_get_nodes_by_attribute_key(myhtml_node_tree(node), NULL, node, search, strlen(search), NULL);
      break;
    case V:
      result = myhtml_get_nodes_by_attribute_value(myhtml_node_tree(node), NULL, node, false, search, strlen(search), value, strlen(value), NULL);
      break;
    default:
      std::cout << "Warning: Incorrect node search type (Valid values: T, A, V)." << std::endl;
  }
  // TODO: Deal with empty results
  nodes.reserve(result->length);
  for (size_t i = 0; i < result->length; i++) {
    nodes.emplace_back(result->list[i]);
  }
  return nodes;
}

const std::string Node::Attribute(const char *key) const {
  return myhtml_attribute_value(myhtml_attribute_by_key(node, key, strlen(key)), NULL);
}

const std::string Node::Content() const {
  mycore_string_raw_t str = {0};
  myhtml_serialization(node, &str);
  std::string content = str.data;

  /* Release resources */
  mycore_string_raw_destroy(&str, false);
  return content;
}

/* PARSER CLASS */

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
  /* Parse HTML document and create node tree */
  myhtml_parse(tree, MyENCODING_UTF_8, data, strlen(data));

  /* Store result */
  mycore_string_raw_t str = {0};
  myhtml_serialization_tree_buffer(myhtml_tree_get_document(tree), &str);
  buffer = str.data;

  /* Release resources */
  mycore_string_raw_destroy(&str, false);

  /*
   * Memory used is always equal to the largest HTML parsed.
   * Therefore, if the file is too big, we build a new tree.
   */
  if(strlen(data) > 5000000) {
    myhtml_tree_destroy(tree);
    myhtml_tree_init(tree, parser);
  }
}

void Parser::Title(){
  //myhtml_collection_t *result = myhtml_get_nodes_by_name(tree, NULL, "title", 5, NULL);
  //myhtml_collection_t *result = myhtml_get_nodes_by_attribute_key(tree, NULL, NULL, "data-ue-u", 9, NULL);
  NodeSet result = Root().Search(V, "name", "twitter:card");

  std::cout << result[0].Attribute("content") << std::endl;

  /*Node n(result->list[0]);
  std::cout << n.Content() << std::endl;*/

  /*Node n(*(result->list));
  std::vector<Node> c = n.Children();
  std::cout << c.size() << std::endl;*/
}
