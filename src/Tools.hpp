#pragma once

#include <time.h>
#include <string>
#include <algorithm>

/**
 * Simple namespace to provide String manipulation functionality.
 */
 namespace String
 {
  /**
   * Strip string (delete initial and trailing whitespace)
   */

  void lstrip(std::string &s);       // Trim from start in place
  void rstrip(std::string &s);       // Trim from end in place
  void strip(std::string &s);        // Trim from both ends in place
  std::string stripc(std::string s); // Trim from both ends (copy)
 };

/**
 * Simple namespace to provide Date and Time functionalities to the program.
 * Dates in PFDB are saved as UNIX times.
 */
namespace DateTime
{

};
