int webserver_core(int mailOpt, char *user, int email_id, char *mail_msg, char *rcpt_user, char *html_response, int server_fd);

int validateUser(char *user);
int validateMailId(char *user, uint16_t mailId);
int downloadEmail(char *user, char *rcpt_to, uint16_t mailId, char *msg, int server_fd);
int send_email(char * user, char *rcpt_user, char *mail_msg, int server_fd);
int deleteEmail(char *user, char *rcpt_to, uint16_t mailId, char *msg, int server_fd);

int retrieveMailHeader(char *user, char *rcpt_user, int server_fd);
