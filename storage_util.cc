#include "storage_util.h"
#include <string.h>
#include <unistd.h>
#include <algorithm>  // find
#include <iostream>
#include <sstream>  // istringstream
#include "data_types.h"
#define MAX_FOLDER_INFO_LENGTH 16184
static void split(std::vector<std::string>& tokens, const std::string& s,
                  char delimiter) {
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }
}
std::string replace_first_occurrence(std::string& s,
                                     const std::string& toReplace,
                                     const std::string& replaceWith) {
  std::size_t pos = s.find(toReplace);
  if (pos == std::string::npos) return s;
  return s.replace(pos, toReplace.length(), replaceWith);
}
std::string replace_all_occurrences(std::string& s,
                                    const std::string& toReplace,
                                    const std::string& replaceWith) {
  std::size_t pos = s.find(toReplace);
  while (true) {
    if (pos == std::string::npos) return s;
    s = s.replace(pos, toReplace.length(), replaceWith);
    pos = s.find(toReplace);
  }
}
void request_file_names(std::vector<std::string>& filenames,
                        const std::string& path, const std::string& username,
                        const std::string& folder_name, BackendRelay* BR) {
  if (path.size() > 1023 || username.size() > 31 || folder_name.size() > 255) {
    std::cerr << "request file names param too large." << std::endl;
    return;
  }
  get_folder_content_request req;
  memset(&req, 0, sizeof(req));
  strcpy(req.prefix, "getfolder");
  strcpy(req.directory_path, path.c_str());
  strcpy(req.username, username.c_str());
  strcpy(req.folder_name, folder_name.c_str());
  std::string response_line =
      BR->sendFolderRequest(&req, MAX_FOLDER_INFO_LENGTH);
  if (!response_line.compare("")) return;
  split(filenames, response_line, '~');
}

void generate_display_list(std::string& to_replace,
                           const std::string& current_path,
                           const std::vector<string>& filenames) {
  for (auto& item : filenames) {
    char item_type = item[0];
    string real_fname = item.substr(1);
    if (item_type == 'D') {
      to_replace += "<div><a href=\"/ls" + current_path + "/" + real_fname +
                    "\">View directory " + real_fname + "</a></div>" +
                    "<form action=\"/rmfolder" + current_path + "/" +
                    real_fname +
                    "\" method=\"post\"><div><input type =\"submit\" "
                    "value=\"Remove Folder\"></div></form>";
    } else if (item_type == 'F') {
      to_replace += "<div><a href=\"/download" + current_path + "/" +
                    real_fname + "\">Download file " + real_fname +
                    "</a></div>";
    }
  }
}
void split_filename(const std::string& str, std::string& path,
                    std::string& folder) {
  std::size_t found = str.find_last_of("/\\");
  path = str.substr(0, found);
  folder = str.substr(found + 1);
}
bool request_file_download(const std::string& username,
                           const std::string& current_path,
                           const std::string& fname, BackendRelay* BR) {
  std::vector<std::string> ls;
  string path, folder;
  split_filename(current_path, path, folder);
  request_file_names(ls, path, username, folder, BR);
  if (std::find(ls.begin(), ls.end(), fname.c_str()) == ls.end()) {
    // fname in the list
    return false;
  }
  // if file exists
  get_file_request req;
  memset(&req, 0, sizeof(req));
  strcpy(req.prefix, "getfile");
  strcpy(req.directory_path, current_path.c_str());
  strcpy(req.username, username.c_str());
  strcpy(req.filename, fname.c_str());
  BR->sendFileRequest(&req);
  return true;
}
void download_file_chunk(std::string& chunk, bool* read_ready, size_t* f_len,
                         BackendRelay* BR) {
  get_file_response resp;
  BR->recvChunk(&resp);
  if (resp.chunk[CHUNK_SIZE - 1] != 0) {
    // full chunk
    char* tmp = new char[CHUNK_SIZE + 1];
    memcpy(tmp, resp.chunk, CHUNK_SIZE);
    tmp[CHUNK_SIZE] = 0;
    chunk = tmp;
    delete[] tmp;
  } else {
    chunk = resp.chunk;
  }
  *f_len = resp.f_len;
  *read_ready = resp.has_more;
}

bool upload_next_part(int* total_body_read, std::string& filename, int sock,
                      std::string& boundary, std::string& username,
                      string& directory_path, BackendRelay* BR) {
  char* buffer = new char[MAX_CHUNK_PLUS_SIZE + 1];
  int buffer_size;
  std::string buff_str;
  while ((buffer_size = read(sock, buffer, MAX_CHUNK_PLUS_SIZE)) ==
         MAX_CHUNK_PLUS_SIZE) {
    buffer[buffer_size] = 0;
    buff_str += buffer;
  };
  buffer[buffer_size] = 0;
  buff_str += buffer;
  delete[] buffer;

  // verify that boundary is at the end
  size_t begin_boundary = buff_str.rfind(boundary);
  size_t buff_str_size = buff_str.size();
  bool ret_val;
  // Case 1: this data is not the last part, because it ends with boundary
  if (begin_boundary + boundary.size() == buff_str_size) {
    ret_val = true;
  }
  // Case 2: last part, ends with boundary plus "--"
  else if (buff_str[buff_str_size - 1] == '-' &&
           buff_str[buff_str_size - 2] == '-' &&
           begin_boundary + boundary.size() + 2 == buff_str_size) {
    ret_val = false;
  } else {
    std::cerr << "Programming Error from upload_next_part! : This part of data "
                 "does not end with boundary ! begin_boundary: "
              << begin_boundary << " boundary size: " << boundary.size()
              << " full length: " << buff_str_size << std::endl;
    *total_body_read = 0;
    return false;
  }

  size_t fname_start_inx, fname_end_inx, data_start_inx, data_end_inx;
  fname_start_inx = buff_str.find("filename=\"") + 10;
  fname_end_inx = buff_str.find("\"", fname_start_inx);
  filename = buff_str.substr(fname_start_inx, fname_end_inx - fname_start_inx);
  // got filename, get data
  data_start_inx = buff_str.find("\r\n\r\n", fname_end_inx) + 4;
  data_end_inx = buff_str.find("\r\n", data_start_inx);
  std::string data =
      buff_str.substr(data_start_inx, data_end_inx - data_start_inx);
  buff_str.clear();
  *total_body_read += data.size();
  // got username, got directory_path, start send
  size_t size_yet_to_send = data.size();
  int chunk_id = 0;
  bool confirmed;
  while (size_yet_to_send > CHUNK_SIZE) {
    confirmed = BR->sendChunk(username, directory_path, filename,
                              data.substr(chunk_id * CHUNK_SIZE, CHUNK_SIZE),
                              CHUNK_SIZE, true);
    chunk_id++;
    size_yet_to_send -= CHUNK_SIZE;
  }
  // last send chunk
  confirmed = BR->sendChunk(username, directory_path, filename,
                            data.substr(CHUNK_SIZE * chunk_id),
                            size_yet_to_send, false);
  // TODO: handle Backend confirmation
  return ret_val;
};