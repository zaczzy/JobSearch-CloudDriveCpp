/*
 * util.cc
 *
 *  Created on: Sep 14, 2019
 *      Author: cis505
 */
#include "util.h"
#include <iostream>

void exit_guard(bool err, const char *message) {
  if (err) {
    std::cerr << message << std::endl;
    exit(-1);
  }
}
int safe_stoi(std::string &n_str, const char *message) {
  try {
    return std::stoi(n_str);
  } catch (const std::invalid_argument &ia) {
    std::cerr << message << n_str << std::endl;
    return -1;
  }
}

void print_vector(std::vector<long long> &vec) {
  for (auto num : vec) std::cout << num << std::endl;
}
