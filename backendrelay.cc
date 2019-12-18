#include "backendrelay.h"
#include <cstring>
#include "servercommon.h"
/*
 * Constructor
 */
BackendRelay::BackendRelay(int sock) : masterSock(sock) {
  backendSock = -1;
  mutex_sock = PTHREAD_MUTEX_INITIALIZER;
}

/*
 * Destructor
 */
BackendRelay::~BackendRelay() {}

/*
 * Request new backend socket from master
 */
void BackendRelay::setNewBackendSock(string username) {
  if (backendSock == -1) {
    //Gimme new backend (indicate am fes)
    unsigned short config_pair[2] = {0,0};
    int i = write(masterSock, config_pair, sizeof(config_pair));
    //read() error
    if (i < -1)
      die("write() fail to masterSock", false);
    //client closed connection
    if (i == 0)
      die("masterSock closed", false);
  }

  //Send username
  char buff[BUFF_SIZE];
  sprintf(buff, "%s", (char *)username.c_str());
  int i = write(masterSock, buff, sizeof(buff));
  if (i < -1)
    die("write() fail to masterSock", false);
  if (i == 0)
    die("masterSock closed", false);

  if (VERBOSE)
    cout << "master written bytes: " << i << endl;

	//Backend gimme'd
	struct sockaddr_in newAddr;
	i = read(masterSock, &newAddr, sizeof(newAddr));
	if (i < -1)
		die("read() fail to masterSock", false);
	if (i == 0)
		die("masterSock closed", false);

  if (VERBOSE)
	  cout << "master read" << endl;

  cout << inet_ntoa(newAddr.sin_addr) << "\n"<< ntohs(newAddr.sin_port) << endl;

  if (backendSock > 0)
    close(backendSock);

	//Create new backend sock
	backendSock = socket(AF_INET, SOCK_STREAM, 0);
	if (backendSock < 0)
		die("socket() failed", -1);

	cout << "backend made " << backendSock << endl;

	//Reusable
	int enable = 1;
	if (setsockopt(backendSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		die("setsockopt(SO_REUSEADDR) failed", false);
	if (setsockopt(backendSock, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
		die("setsockopt(SO_REUSEPORT) failed", false);

	//Connect
	if (connect(backendSock, (struct sockaddr*)&newAddr, sizeof(newAddr)) < 0)
		die("connect failed", false);
//	return backendSock;

	//Clear welcome message from backend socket
	memset(buff, 0, sizeof(buff));
	read(backendSock, buff, sizeof(buff));
	if (VERBOSE)
		fprintf(stderr, "%s\n", buff);

	if (VERBOSE)
		fprintf(stderr, "S: connected to [%d]! (B)\n", backendSock);
}

/*
 * Send command to backend (ritika)
 */
string BackendRelay::sendCommand(string command) {
  char* c_command = (char*)command.c_str();
  char buff[BUFF_SIZE];
  write(backendSock, c_command, strlen(c_command));
  if (VERBOSE) fprintf(stderr, "[%d][BACK] S: %s\n", backendSock, c_command);
  read(backendSock, buff, sizeof(buff));
  if (VERBOSE) fprintf(stderr, "[%d][BACK] C: %s\n", backendSock, buff);
  string result = buff;
  return result;
}

int BackendRelay::getSock() { return backendSock; }

// RESPONSE be like "Ffile1~Ddir1~Ffile2~" or "~" when empty
string BackendRelay::sendFolderRequest(const get_folder_content_request* req,
                                       size_t max_resp_len) {
  write(backendSock, req, sizeof(*req));
  char* buff = new char[max_resp_len];
  int buff_size = read(backendSock, buff, max_resp_len);
  string result;
  if ((size_t)buff_size < max_resp_len) {
    buff[buff_size] = '\0';
    result = buff;
    if (buff_size == 1 && buff[0] == '~') {
      return "";
    } else if (buff[buff_size - 1] != '~') {
      cerr<< "getFolderContentRequest response malformated" << endl;
      cerr << "|" << result << "|" << endl;
      result = "";
    }
    printf("folder content: %s\n", buff);
  } else {
    fprintf(stderr, "sendFolderRequest response overflow\n");
    result = "";
  }
  delete[] buff;
  return result;
}
void BackendRelay::sendFileRequest(const get_file_request* req) {
  write(backendSock, req, sizeof(*req));
}
bool BackendRelay::sendChunk(const string& username,
                             const string& directory_path,
                             const string& filename, const string& data,
                             const size_t chunk_len) {
  put_file_request req;
  memset(&req, 0, sizeof(req));
  strcpy(req.prefix, "putfile");
  strcpy(req.username, username.c_str());
  strcpy(req.directory_path, directory_path.c_str());
  strcpy(req.filename, filename.c_str());
  memcpy(req.data, data.c_str(), chunk_len);
  req.chunk_len = (uint64_t)chunk_len;
  cout << "UN: " << username << " DP: " << directory_path << " FN: " << filename
       << " DATA: <" << data << ">CLEN: " << (uint64_t)chunk_len << endl;
  printf("%lu\n", sizeof(put_file_request));
  printf("%lu\n", sizeof(req));
  write(backendSock, &req, sizeof(req));
  char buff[50];
  int resp_size = read(backendSock, &buff, 50);
  buff[resp_size] = '\0';
  if (strncmp(buff, "+OK", 3) == 0) {
    return true;
  }
  fprintf(stderr, "something wrong with sendChunk\n");
  return false;
}
// TODO: ask ritika about confirm
bool BackendRelay::createFolderRequest(const create_folder_request* req) {
  write(backendSock, req, sizeof(*req));
  char* confirm = new char[1024];
  int rlen = read(backendSock, confirm, 1024);
  confirm[rlen] = '\0';
  printf("mkfolder confirm: %s\n", confirm);
  // bool retval = strncmp(confirm, "+OK", 3);
  delete[] confirm;
  return true;
}
// TODO: ask ritika about confirm
bool BackendRelay::removeFolderRequest(
    const delete_folder_content_request* req) {
  write(backendSock, req, sizeof(*req));
  char* confirm = new char[1024];
  int rlen = read(backendSock, confirm, 1024);
  confirm[rlen] = '\0';
  printf("confirm: %s\n", confirm);
  // bool retval = strncmp(confirm, "+OK", 3) == 0;
  delete[] confirm;
  return true;
}
void BackendRelay::recvChunk(get_file_response* resp) {
  read(backendSock, resp, sizeof(*resp));
};
