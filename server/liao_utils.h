#ifndef _LIAO_UTILS_H
#define _LIAO_UTILS_H

#include "mfile.h"

#include "confparser.h"
#include "dictionary.h"
#include "liao_command.h"

int set_to_mc(char *mc_ip, int mc_port, int mc_timeout, char *mc_key, char *mc_value);
int get_to_mc(char *mc_ip, int mc_port, int mc_timeout, char *mc_key, char *mc_value, size_t mc_value_size);
int delete_from_mc(char *mc_ip, int mc_port, int mc_timeout, char *mc_key);

void init_clientst_item_with_idx(int idx);

void reinit_data_without_free(MFILE *mfp_in);

void online_dump(dictionary *d, int fd);
int send_msg_to_another_process(int parent_w_fd, char *pid, char *fuid, char *fnick, char *type, char *tuid);
int send_socket_cmd(int client_fd, int client_type, char *fuid, char *fnick, char *type, char *tuid);
void send_apn_cmd(struct config_t *config, char *fuid, char *fnick, char *type, char *tuid);

FILE *open_img_with_name(char *img_name, char *tuid, char *fn, size_t fn_size, char *img_full_path, size_t img_full_path_size, char *iname, size_t iname_size);

int write_content_to_file_with_uid(struct config_t *config_st, char *tuid, char *content, size_t content_len, char *file_name, size_t file_name_size);
int write_content_to_file_with_path(char *file_path, char *old_name, char *content, size_t content_len, char *file_name, size_t file_name_size);
int write_content_to_file_with_uid_mfile(struct config_t *config_st,char *tuid, char *content, size_t content_len, MFILE *ftp_in, char *file_name, size_t file_name_size);
int write_b64_content_to_file_with_uid_mfile(char *file_path, char *content, size_t content_len, MFILE *mfp_in, char *old_img_name, char *file_name, size_t file_name_size);

char *get_offline_msg_with_uid(struct config_t *config_st, char *myuid);

int image_resize_v1(char *old_img, char *new_img, int w, int h, int compress_quality);
int image_resize_without_scale(char *old_img, char *new_img, int compress_quality);

char *login_http_api(char *url, char *account, char *password, int get_friend_list, int *result_len);

int write_queue_to_db(struct config_t *config_st, char *tag_type, char *mid, char *fuid, char *fnick, char *fios_token, char *tuid, char *tios_token, char *queue_type, char *queue_file, int queue_size);
int write_queue_to_db_v2(struct config_t *config, char *mid, char *tag_type, char *fuid, char *fnick, char *tuid, char *queue_type, char *queue_file, long long queue_size, char *image_wh);

void clean_mem_buf(char *buf);

int get_avatar_url(struct config_t *config_st, char *uid, char *url, size_t url_size);

void mybyte_copy( register char *to, register unsigned int n, register char *from);

int fast_write(MFILE *mfp, char *buf, size_t buf_len);

int fd_move( int to, int from);
int fd_copy( int to, int from);

void send_to_socket(int sockfd, char *rspsoneBuf, int responseBuf_len);
void send_to_websocket(int sockfd, char *websocket_data, int websocket_data_len);

int user_login(struct config_t *config, int type, char *uid, char *ios_token);
int get_iostoken_logined(struct config_t *config, char *tuid, char *tios_token, size_t tios_token_size);

int sha1_encode(char *src, size_t src_len, char *dst);

int websocket_fmt_data_decode(char *buf, size_t buf_size, char *payload_data, size_t payload_data_size);
char *websocket_fmt_data_encode(char *buf, size_t buf_size, unsigned long *send_data_len);


#endif
