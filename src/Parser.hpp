#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <myhtml/api.h>

/**
 * The search type when performing a search of nodes on the DOM tree.
 * It can be search by Tag (T), by Attribute (A) or by attribute and value (V).
 */
enum SearchType { T, A, V };

/**
 * A node of the HTML DOM tree.
 */
class Node
{
private:
  myhtml_tree_node_t *node;   // Current node in tree
public:
  Node(myhtml_tree_node_t *node);
  ~Node();
  Node(const Node&);

  /* Navigating nodes */
  inline Node Parent() const { return Node(myhtml_node_parent(node)); }
  inline Node Next() const { return Node(myhtml_node_next(node)); }
  inline Node Prev() const { return Node(myhtml_node_prev(node)); }
  std::vector<Node> Children() const;

  /* Searching nodes */
  std::vector<Node> Search(SearchType type, const char* search, const char* value = "") const;

  /* Manipulating nodes */
  const std::string Attribute(const char* key) const;
  const std::string Content() const;
};

// TODO: Make NodeSets vectors of Node*, and have all Node methods return
// Node*, for two reasons:
// 1) Consistency:
// 1.1) With the parser, in which everything is stored as pointers.
// 1.2) Chaining methods with ->.
// 2) Efficiency when resizing vector.
// Problem: How to return such objects? If they are stored in the heap, where to
// store the pointer? When to deallocate the memory? Maybe smart pointers?
typedef std::vector<Node> NodeSet;

/**
 * API to parse HTML by wrapping MyHTML.
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
  inline Node Root() const { return Node(myhtml_tree_get_document(tree)); } // Return root (document) node of tree
  inline void Print() const { std::cout << buffer << std::endl; }

  void Title();

  /**
   * Singleton: Remove possibility of duplicating object.
   */
  Parser(const Parser&) = delete;            // Remove copy constructor
  Parser& operator=(const Parser&) = delete; // Remove asignment operator
};
