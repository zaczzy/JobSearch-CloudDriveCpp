//
// Created by Zachary Zhao on 10/8/19.
//
#ifndef INC_505HW2_THREAD_HANDLER_H
#define INC_505HW2_THREAD_HANDLER_H
int my_handler(int fd);
void write_sock(int fd, const struct server_netconfig *server_config);
void write_primary(int fd, int primary_id);
#endif  // INC_505HW2_THREAD_HANDLER_H
