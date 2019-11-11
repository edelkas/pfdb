#pragma once

#include <string>
#include <myhtml/api.h>

/**
 * API to parse HTML by wrapping MyHTML
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
  void Print() const;
};
