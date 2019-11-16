#pragma once

int prepare_socket(int port_no);
void send_msg_to_socket(const char* msg, int msg_len, int client_fd);
