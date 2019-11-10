#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "DateTime.hpp"
#include "Website.cpp"

typedef std::unordered_map<std::string, std::string> hash;
typedef std::unordered_map<std::string, std::string> numHash;

class Film
{
private:
  /*
   * BASIC MEMBERS
   */
  hash ID;                // ID of the film in each Website
  hash title;             // "Original", "Spanish", "English", ...
  hash synopsis;          // Plot summary from each Website

  /*
   * NUMERIC MEMBERS
   */
  unsigned int duration;  // Duration of the film in minutes
  unsigned int year;      // Year of release of the film
  unsigned int budget;    // Budget of the film in dollars
  unsigned int gross;     // Gross revenue of the film in dollars

  /*
   * CREW
   *
   * The following members are hashes since we store:
   * Name -> Role / additional info regarding such role.
   */
  hash cast;              // Actors and the characters played
  hash directors;
  hash writers;
  hash producers;
  hash composers;         // Music, soundtrack composers
  hash cinematographers;  // Cinematography
  hash editors;           // Film editing
  hash castings;          // Casting directors
  hash companies;         // Production companies

  /*
   * Placement statistics of the film in each of the Websites.
   */
  numHash votes;
  numHash ratings;
  numHash ranks;

  std::vector<std::string> genres;
  std::vector<std::string> countries;
  std::vector<std::string> languages;

  /*
   * Dates saved in UNIX time.
   *
   * Standard dates:
   * - Added:   When the film was added to the database. Should be only one.
   * - Updated: When the information of the film was last updated from the web.
   * - Seen:    When the film was seen. Could be multiple times.
   */
  std::unordered_map<std::string, std::vector<int>> dates;

  std::string color;                 // B&W, Color.
  std::string certification;         // USA rating: PG, R, ...

  /*
   * Personal information.
   */
  unsigned int personal_rating;     // Out of 10, 1 decimal place.
  std::string personal_comment;
  std::vector<std::string> reasons; // For having the film in the database.

public:
  Film();
  ~Film();
};
