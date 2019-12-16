#include <inttypes.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <locale>
#include "server_config_store.h"
// using namespace std;

// trim from start (in place)
static inline void ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                  [](int ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](int ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
  ltrim(s);
  rtrim(s);
}

inline bool operator<(const server_netconfig &lhs,
                      const server_netconfig &rhs) {
  return lhs.key < rhs.key;
}
// membership data structure
map<int, vector<server_netconfig>> groups;
map <int, pair<int, size_t>> sock2servid;
template <typename Out>
static void split(const std::string &s, char delim, Out result) {
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
void read_config_file(char *config_name) {
  // read config file
  ifstream infile(config_name);
  string line;
  string ip;
  uint16_t port;
  int line_count = 1;
  while (getline(infile, line)) {
	int id_count = 1;
    vector<string> members = split(line, ',');
    vector<server_netconfig> temp_vec;  // value
    for (auto &member : members) {
      server_netconfig tmp;
      tmp.key = id_count++;
      tmp.serv_addr.sin_family = AF_INET;  // IPv4
      string ip_str = member.substr(0, member.find(":"));
      trim(ip_str);
      tmp.serv_addr.sin_addr.s_addr = inet_addr(ip_str.c_str());
      uint16_t tmp_port;
      string port_str = member.substr(member.find(':') + 1);
      trim(port_str);
      if (!str_to_uint16(port_str.c_str(), &tmp_port)) {
        exit(-1);
      }
      tmp.serv_addr.sin_port = htons(tmp_port);
      temp_vec.emplace_back(tmp);
    }
    groups.insert({line_count++, temp_vec});
  }
}
