#ifndef FETCH_HPP
#define FETCH_HPP

#include <vector>
#include <string>

#include "image.hpp"

std::vector<image> fetch_cover(std::string path, std::string artist, std::string album);

#endif

