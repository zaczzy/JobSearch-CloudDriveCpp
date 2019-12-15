#ifndef AE47C3FC_4A89_4469_8EC1_F2CB7ADE2802
#define AE47C3FC_4A89_4469_8EC1_F2CB7ADE2802
#include <map>
#include <vector>
#include <arpa/inet.h>
#include <netinet/in.h>
using namespace std;
struct server_netconfig {
  int key;
  struct sockaddr_in serv_addr;
};
extern map<int, vector<server_netconfig>> groups;
extern vector<server_netconfig> frontends;
void read_config_file(char *config_name);
#endif /* AE47C3FC_4A89_4469_8EC1_F2CB7ADE2802 */