#pragma once

#include <iostream>
#include <string>
#include <myhtml/api.h>

/**
 * API to parse HTML by wrapping MyHTML
 *
 * This class is a singleton, only one instance can be created.
 */
class Parser
{
private:
  myhtml_t *parser;
  myhtml_tree_t *tree;
  std::string buffer;
public:
  Parser();
  ~Parser();

  void Parse(const char *data);
  inline void Print() const { std::cout << buffer << std::endl; }

  /**
   * Singleton: Remove possibility of duplicating object.
   */
  Parser(const Parser&) = delete;            // Remove copy constructor
  Parser& operator=(const Parser&) = delete; // Remove asignment operator
};
