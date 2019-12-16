/*
 * util.h
 *
 *  Created on: Sep 14, 2019
 *      Author: cis505
 */

#ifndef UTIL_H_
#define UTIL_H_
#include <iostream>
#include <string>
#include <vector>
void exit_guard(bool err, const char* message);
#define panic(a...) do { fprintf(stderr, a); fprintf(stderr, "\n"); exit(1); } while (0) 
void print_vector(std::vector<long long>& vec);
int safe_stoi(std::string& n_str, const char* message);

#endif /* UTIL_H_ */
