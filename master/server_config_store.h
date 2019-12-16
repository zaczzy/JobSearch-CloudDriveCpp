#ifndef AE47C3FC_4A89_4469_8EC1_F2CB7ADE2802
#define AE47C3FC_4A89_4469_8EC1_F2CB7ADE2802
#include <arpa/inet.h>
#include <netinet/in.h>
#include <map>
#include <utility>
#include <vector>
using namespace std;
enum Status { Alive = 0, Dead };
struct server_netconfig {
  int key;
  struct sockaddr_in serv_addr;
  Status status;
};
extern map<int, vector<server_netconfig>> groups;
extern map <int, pair<int, size_t>> sock2servid;
void read_config_file(char *config_name);
#endif /* AE47C3FC_4A89_4469_8EC1_F2CB7ADE2802 */