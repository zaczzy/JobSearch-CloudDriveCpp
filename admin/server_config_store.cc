#include "server_config_store.h"
#include <inttypes.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
using namespace std;
map<int, vector<server_netconfig>> groups;
vector<server_netconfig> frontends;
// trim from start (in place)
static inline void ltrim(string &s) {
  s.erase(s.begin(),
          find_if(s.begin(), s.end(), [](int ch) { return !isspace(ch); }));
}

// trim from end (in place)
static inline void rtrim(string &s) {
  s.erase(
      find_if(s.rbegin(), s.rend(), [](int ch) { return !isspace(ch); }).base(),
      s.end());
}

// trim from both ends (in place)
static inline void trim(string &s) {
  ltrim(s);
  rtrim(s);
}

inline bool operator<(const server_netconfig &lhs,
                      const server_netconfig &rhs) {
  return lhs.key < rhs.key;
}
// membership data structure
template <typename Out>
static void split(const string &s, char delim, Out result) {
  istringstream iss(s);
  string item;
  while (getline(iss, item, delim)) {
    *result++ = item;
  }
}
static vector<string> split(const string &s, char delim) {
  vector<string> elems;
  split(s, delim, back_inserter(elems));
  return elems;
}
static bool str_to_uint16(const char *str, uint16_t *res) {
  char *end;
  errno = 0;
  intmax_t val = strtoimax(str, &end, 10);
  if (errno == ERANGE || val < 0 || val > UINT16_MAX || end == str ||
      *end != '\0')
    return false;
  *res = (uint16_t)val;
  return true;
}
static void prepare_server_netconfig(server_netconfig *tmp, int *id_count,
                                     const string &member) {
  tmp->key = *id_count;
  *id_count++;
  tmp->serv_addr.sin_family = AF_INET;  // IPv4
  string ip_str = member.substr(0, member.find(":"));
  trim(ip_str);
  tmp->serv_addr.sin_addr.s_addr = inet_addr(ip_str.c_str());
  uint16_t tmp_port;
  string port_str = member.substr(member.find(':') + 1);
  trim(port_str);
  if (!str_to_uint16(port_str.c_str(), &tmp_port)) {
    exit(-1);
  }
  tmp->serv_addr.sin_port = htons(tmp_port);
}

void read_config_file(char *config_name) {
  // read config file
  ifstream infile(config_name);
  string line;
  string ip;
  uint16_t port;
  int id_count = 0;
  int line_count = 0;
  bool frontend_config_start = false;
  while (getline(infile, line)) {
    if (!line.compare("-")) {
      frontend_config_start = true;
    } else {
      vector<string> members = split(line, ',');
      // frontend config
      if (frontend_config_start) {
        id_count = 0;
        for (auto &member : members) {
          server_netconfig tmp;
          prepare_server_netconfig(&tmp, &id_count, member);
          frontends.emplace_back(tmp);
        }
      }  // backend config
      else {
        vector<server_netconfig> temp_vec;  // value
        for (auto &member : members) {
          server_netconfig tmp;
          prepare_server_netconfig(&tmp, &id_count, member);
          temp_vec.emplace_back(tmp);
        }
        groups.insert({line_count++, temp_vec});
      }
    }
  } 
}