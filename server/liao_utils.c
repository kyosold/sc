#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libmemcached/memcached.h>
#include "mysql.h"
#include <sys/epoll.h>  
#include <curl/curl.h>
#include <wand/MagickWand.h>
#include <openssl/sha.h>

#include "liao_log.h"
#include "liao_utils.h"
#include "liao_command.h"

#include "confparser.h"
#include "dictionary.h"


//extern char mc_timeout[512];
//extern char max_works[512];
//extern struct clients *client_st;
//extern struct childs *child_st;
//extern struct confg_st config_st;


/*void sig_catch(int sig, void (*f) () )
{
    struct sigaction sa; 
    sa.sa_handler = f;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, (struct sigaction *) 0); 
}

void sig_block(sig)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigaddset(&ss, sig);
	sigprocmask(SIG_BLOCK, &ss, (sigset_t *) 0);	
}

void sig_unblock(sig)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigaddset(&ss, sig);
	sigprocmask(SIG_UNBLOCK, &ss, (sigset_t *) 0);
}*/


// return:
// 	0:succ	1:fail
int set_to_mc(char *mc_ip, int mc_port, int mc_timeout, char *mc_key, char *mc_value)
{
	size_t nval = 0;
    uint32_t flag = 0;
    char *result = NULL;
    int ret = 0;
    
    memcached_st        *memc = memcached_create(NULL);
    memcached_return    mrc;
    memcached_server_st *mc_servers;
    
    memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_NO_BLOCK, 1);
    memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, mc_timeout);
    
    mc_servers= memcached_server_list_append(NULL, mc_ip, mc_port, &mrc);
    log_debug("memcached_server_list_append[%d]:%s:%d", mrc, mc_ip, mc_port);
    
    if (MEMCACHED_SUCCESS == mrc) {
    	mrc= memcached_server_push(memc, mc_servers);
    	memcached_server_list_free(mc_servers);
    	if (MEMCACHED_SUCCESS == mrc) {
    		char mc_value_len = strlen(mc_value);
    		mrc = memcached_set(memc, mc_key, strlen(mc_key), mc_value, mc_value_len, 0, (uint32_t)flag);
    		if (MEMCACHED_SUCCESS == mrc) {
    			log_debug("set mc key:%s val:%s succ", mc_key, mc_value);
    			ret = 0;;
    		} else {
    			log_error("set mc key:%s val:%s failed:%s", mc_key, mc_value, memcached_strerror(memc, mrc));
    			ret = 1;
    		}
    		
    		memcached_free(memc);
    		return ret; 
    	}
    } 
    
    log_error("set_to_mc:%s:%d connect fail:%s", mc_ip, mc_port, memcached_strerror(memc, mrc));
	
	memcached_free(memc);
	
	return 1;
}

// return:
// 	0:succ	1:fail
int delete_from_mc(char *mc_ip, int mc_port, int mc_timeout, char *mc_key)
{
	size_t nval = 0;
    uint32_t flag = 0;
    char *result = NULL;
    int ret = 0;
    
    memcached_st        *memc = memcached_create(NULL);
    memcached_return    mrc;
    memcached_server_st *mc_servers;
    
    memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_NO_BLOCK, 1);
    memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, mc_timeout);
    
    mc_servers= memcached_server_list_append(NULL, mc_ip, mc_port, &mrc);
    log_debug("memcached_server_list_append[%d]:%s:%d", mrc, mc_ip, mc_port);
    
    if (MEMCACHED_SUCCESS == mrc) {
    	mrc= memcached_server_push(memc, mc_servers);
    	memcached_server_list_free(mc_servers);
    	if (MEMCACHED_SUCCESS == mrc) {
			mrc = memcached_delete(memc, mc_key, strlen(mc_key), 0);
    		if (MEMCACHED_SUCCESS == mrc) {
    			log_debug("delete mc key:%s succ", mc_key);
    			ret = 0;
    		} else {
    			log_error("delete mc key:%s failed:%s", mc_key, memcached_strerror(memc, mrc));
    			ret = 1;
    		}
    		
    		memcached_free(memc);
    		return ret; 
    	}
    } 
    
    log_error("set_to_mc:%s:%d connect fail:%s", mc_ip, mc_port, memcached_strerror(memc, mrc));
	
	memcached_free(memc);
	
	return 1;
}


// return:
// 	0:succ	1:fail
int get_to_mc(char *mc_ip, int mc_port, int mc_timeout, char *mc_key, char *mc_value, size_t mc_value_size)
{
	size_t nval = 0;
    uint32_t flag = 0;
    char *result = NULL;
    int ret = 0;
    
    memcached_st        *memc = memcached_create(NULL);
    memcached_return    mrc;
    memcached_server_st *mc_servers;
    
    memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_NO_BLOCK, 1);
    memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, mc_timeout);
    
    mc_servers= memcached_server_list_append(NULL, mc_ip, mc_port, &mrc);
    log_debug("memcached_server_list_append[%d]:%s:%d", mrc, mc_ip, mc_port);
    
    if (MEMCACHED_SUCCESS == mrc) {
    	mrc= memcached_server_push(memc, mc_servers);
    	memcached_server_list_free(mc_servers);
    	if (MEMCACHED_SUCCESS == mrc) {
    		result = memcached_get(memc, mc_key, strlen(mc_key), (size_t *)&nval, &flag, &mrc);
    		if (MEMCACHED_SUCCESS == mrc) {
    			log_debug("get mc key:%s val:%s succ", mc_key, result);
    			
    			snprintf(mc_value, mc_value_size, "%s", result);
    			
    			ret = 0;
    		} else {
    			log_error("get mc key:%s val:%s failed:%s", mc_key, mc_value, memcached_strerror(memc, mrc));
    			
    			ret = 1;
    		}
    		
    		if (result != NULL) {
    			free(result);
    			result = NULL;
    		}
    			
    		memcached_free(memc);

    		return ret; 
    	}
    } 
    
    log_error("set_to_mc:%s:%d connect fail:%s", mc_ip, mc_port, memcached_strerror(memc, mrc));
	
	memcached_free(memc);
	
	return 1;
}


void online_dump(dictionary *d, int fd)
{
	int     i;
	int		nwrite = 0;
	char 	buf[1024] = {0};
	if (d==NULL || fd<0) return ;
	if (d->n<1) {
		write(fd, "empty dictionary\n", strlen("empty dictionary\n"));
		return ;
	}
	for (i=0 ; i<d->size ; i++) {
		if (d->key[i]) {
			snprintf(buf, sizeof(buf), "%20s\t[%s]\n", 
														d->key[i],
														d->val[i] ? d->val[i] : "UNDEF");
			int buf_len = strlen(buf);
			int n = buf_len;
			while (n > 0) {
				nwrite = write(fd, buf + buf_len - n, n);
				if (nwrite < n) {
					if (nwrite == -1 && errno != EAGAIN) {
						log_error("write to fd[%d] failed:[%d]%s", fd, errno, strerror(errno));
					}
					break;
				}
				n -= nwrite;
			}
		}
	}
}



/*void init_clientst_item_with_idx(int idx)
{
	if (idx < 0 )
		return;

	client_st[idx].used = 0;
    client_st[idx].fd = 0;
    memset(client_st[idx].uid, '0', strlen(client_st[idx].uid));
    memset(client_st[idx].ios_token, '0', strlen(client_st[idx].ios_token));

	struct data_st *send_pdt = client_st[idx].send_data;
	struct data_st *recv_pdt = client_st[idx].recv_data;

	if (send_pdt->data) {
		free(send_pdt->data);
		send_pdt->data = NULL;
	}
	send_pdt->data_size = 0;
	send_pdt->data_len = 0;

	if (recv_pdt->data) {
		free(recv_pdt->data);
		recv_pdt->data = NULL;
	}
	recv_pdt->data_size = 0;
	recv_pdt->data_len = 0;

}*/


// 获取一个新的未使用的在client_st列表中的索引
// return: 
// 	-1 为未得到(可能满了)
/*int new_idx_from_child_st(struct childs *child_st, struct confg_st *config_st)
{
	int idx = -1;
	int i = 0;
	for (i=0; i<(atoi(config_st->max_works) + 1); i++) {
		if (child_st[i]->used == 0) {
			idx = i;
			break;
		}
	}

	return idx;
}*/


// 根据uid取所在索引
// return: -1 为不存在
/*int get_idx_with_uid(char *uid)
{
	int idx = -1;
	int i = 0;
	for (i=0; i<(atoi(config_st.max_works) + 1); i++) {
		if (strcasecmp(client_st[i].uid, uid) == 0) {
			idx = i;
			break;
		}
	}

	return idx;
}*/


/*int get_idx_with_sockfd(int sockfd, struct childs *child_st)
{
	int idx = -1;
	int i = 0;
	for (i=0; i<(atoi(config_st.max_works) + 1); i++) {
		if (child_st[i].fd_in == sockfd && child_st[i].used == 1) {
			idx = i;
			break;
		}
	}

	return idx;
}*/


void reinit_data_without_free(MFILE *mfp_in)
{
	mclose(mfp_in);
	mfp_in = NULL;
}

int get_avatar_url(struct config_t *config, char *uid, char *url, size_t url_size)
{
	int n = 0;
	if (*(config->queue_host) != '\0') {
		n = snprintf(url, url_size, "%s/%s/avatar/s_avatar.jpg", config->queue_host, uid);
	} else {
		n = snprintf(url, url_size, "%s/%s/avatar/s_avatar.jpg", DEF_QUEUEHOST, uid);
	}
	return n;
}



// 说明:
// 	该函数用于使用非阻塞写数据
// 	参数:
//		client:	为被写的client sturct *
//		buf: 被写的内容指针
//		buf_size: 被写的内容长度
//
// return:
// 	-1: 系统错误
/*int safe_write(struct clients *client, char *buf, size_t buf_size)
{
	struct data_st *send_pdt = client->send_data;

	// 申请空间
	if (send_pdt->data == NULL) {
		send_pdt->data_size = buf_size + 1;
		send_pdt->data = (char *)calloc(1, send_pdt->data_size);
		if (send_pdt->data == NULL) {
			send_pdt->data_size = 0;
			log_error("calloc failed:%s size[%d]", strerror(errno), buf_size + 1);
			return -1;
		}
		send_pdt->data_len = 0;
	} else {
		int is_remem_succ = 1;
		if (send_pdt->data_size < (buf_size + 1)) {
			size_t new_size = (send_pdt->data_size + (buf_size + 1)) * 3;
			int fail_times = 1;
			char *tmp = send_pdt->data;
			while ( (send_pdt->data = (char *)realloc(send_pdt->data, new_size)) == NULL ) {
				if (fail_times >= 4) {
					log_error("we can't realloc memory. ");

					send_pdt->data = tmp;
					is_remem_succ = 0;
					break;
				}

				log_error("realloc fail times[%d], retry it.", fail_times);
				sleep(1);
			}

			if (is_remem_succ == 1) {
				 send_pdt->data_size = new_size;
			}
		}

		memset(send_pdt->data, 0, send_pdt->data_size);

	}

	int n = snprintf(send_pdt->data, send_pdt->data_size, "%s", buf);
	send_pdt->data_len = n;
	send_pdt->data_used = 0;

	log_debug("add send message to queue: fd[%d] msg len:%d:%s", client->fd, send_pdt->data_len, send_pdt->data);

	epoll_event_mod(client->fd, EPOLLOUT);

	return 0;
}*/


int send_msg_to_another_process(int client_fd, char *pid, char *fuid, char *fnick, char *type, char *tuid)
{
	int buf_size = 21 + 4 + strlen(pid) + 6 + strlen(fuid) + 7 + strlen(fnick) + 7 + strlen(type) + 6 + strlen(tuid) + 12 + 128;
	char *pbuf = (char *)calloc(1, buf_size);
	if (pbuf != NULL) {
		int n = snprintf(pbuf, buf_size, "type:SSYSTEM_SENDMSG pid:%s fuid:%s fnick:%s mtype:%s tuid:%s \n", pid, fuid, fnick, type, tuid);
		send_to_socket(client_fd, pbuf, n);

		clean_mem_buf(pbuf);
		buf_size = 0;
	}

	return 0;
}


int send_socket_cmd(int client_fd, int client_type, char *fuid, char *fnick, char *type, char *tuid)
{
	char b64_nick[MAX_LINE] = {0};
	int b64_n = base64_decode(fnick, b64_nick, MAX_LINE);
	if (b64_n <= 0) {
		log_error("base64_decode fail");
		return 1;
	}
	

	char notice_mssage[MAX_LINE * 5] = {0};
	int n = snprintf(notice_mssage, sizeof(notice_mssage), "您收到一条来自 %s 的消息", b64_nick);
	if (n <= 0) {
		log_error("snprintf fail");	
		return 1;
	}

	int notice_msg_size = n * 5;
	char *pnotice_msg = (char *)calloc(1, notice_msg_size);
	if (pnotice_msg == NULL) {
		log_error("calloc faile");
		return 1;
	}

	int b64_size = base64_encode(notice_mssage, n, pnotice_msg, notice_msg_size);
	if (b64_size < 0) {
		log_error("base64_encode fail:%s", notice_mssage);

		clean_mem_buf(pnotice_msg);
		notice_msg_size = 0;

		return 1;		
	}

	int notice_size = strlen(type) + strlen(fuid) + strlen(fnick) + strlen(tuid) + b64_size + 512; 
	char *pnotice = (char *)calloc(1, notice_size);
	if (pnotice != NULL) {
		n = snprintf(pnotice, notice_size, "MESSAGENOTICE %s %s %s %s %s %s", type, fuid, fnick, tuid, pnotice_msg, DATA_END);

		//safe_write(client_t, pnotice, n);
		//send_to_client(parent_w_fd, pnotice, n);
		if (client_type == 1) {
			send_to_websocket(client_fd, pnotice, n);
		} else {
			send_to_socket(client_fd, pnotice, n);
		}

		clean_mem_buf(pnotice);
		notice_size = 0;
	}

	clean_mem_buf(pnotice_msg);
	notice_msg_size = 0;

	return 0;
		
}


void send_apn_cmd(struct config_t *config, char *fuid, char *fnick, char *type, char *tuid)
{
	// 检查收件人是否登录状态
	char tios_token[MAX_LINE] = {0};
	int ret = get_iostoken_logined(config, tuid, tios_token, sizeof(tios_token));
	if (ret == -1) {
		log_error("couldn't get receiver ios_token, so don't send apn");
		return;

	} else if ((ret == 0) || (strlen(tios_token) <= 0)) {
		log_info("receiver had logout, so don't send apn");
		return;
	}

	char cmd[MAX_LINE] = {0};
	snprintf(cmd, sizeof(cmd), "./push.php '%s' '%s' '%s' '%s' '%s'", tios_token, fuid, fnick, type, tuid);
	//snprintf(cmd, sizeof(cmd), "/data1/htdocs/colorful_flags/server_v2/push.php '%s' '%s' '%s' '%s' '%s'", ios_token, fuid, fnick, type, tuid);
	log_debug("APN CMD: %s", cmd);

	FILE *fp;
	char buf[MAX_LINE] = {0};

	fp = popen(cmd, "r");
	if (fp == NULL) {
		log_error("popen failed:%s", strerror(errno));
		return;
	}

	while ( fgets(buf, sizeof(buf), fp) != NULL ) {
		log_info("%s", buf);
	}

	pclose(fp);
}



// 检查目录是否存在,如不存在则创建
int is_dir_exits(char *path)
{
	char dir_name[MAX_LINE] = {0};
	strcpy(dir_name, path);

	int i, len = strlen(dir_name);
	if (dir_name[len - 1] != '/') 
		strcat(dir_name, "/");

	for (i = 1; i<len; i++) {
		if (dir_name[i] == '/') {
			dir_name[i] = '\0';
			if ( access(dir_name, F_OK) != 0 ) {
				umask(0);
				if ( mkdir(dir_name, 0777) == -1) {
					log_error("create dir:%s fail:%s", dir_name, strerror(errno));
					return 1;
				}
			}

			dir_name[i] = '/';
		}
	}

	return 0;
}


// return fd of fn
FILE *open_img_with_name(char *img_name, char *tuid, char *fn, size_t fn_size, char *img_full_path, size_t img_full_path_size, char *img_full_host, size_t img_full_host_size)
{
	FILE *fp;
	char fn_pid[MAX_LINE] = {0};
    int i=0, fd = -1;
    static int seq = 0; 

	snprintf(img_full_path, img_full_path_size, "%s/%s/images/", IMG_PATH, tuid);
	snprintf(img_full_host, img_full_host_size, "%s/%s/images/", IMG_HOST, tuid);

	if ( is_dir_exits(img_full_path) != 0 ) {
        log_error("is_dir_exit fail");
        return NULL;
	}

    (void)umask(033);
    srandom(time(NULL));

    for (i= 0; i< 100; i++) 
    {    
		snprintf(fn, fn_size, "%.5ld%.5u%.3d_%s", time(NULL), getpid(), seq++, img_name);

        snprintf(fn_pid, sizeof(fn_pid), "%s/%s", img_full_path, fn);

		fd = open(fn_pid, O_WRONLY | O_EXCL | O_CREAT, 0755);
        if (fd != -1)
        {    
            fp = fdopen(fd, "wb");
            if(fp)
            {    
                setbuf(fp, fb_pid);
				return fp;	
            }    
            close(fd);
        }    
        log_info("open file:(%s) error:%m", fn_pid);
    }    


    return NULL;
}


// 返回: 
// 	 -1: 失败, other: 写入的字节数
// 这里有两个文件:
// content.txt: 保存聊天消息
// content_idx.txt: 保存聊天消息的索引:
// 		每行一条消息: start_offset, length, file
//
//	queue_path/tuid/
int write_content_to_file_with_uid(struct config_t *config,char *tuid, char *content, size_t content_len, char *file_name, size_t file_name_size)
{
	FILE *fp = NULL;
	int i = 0;
	int fd = 0;
	char file_path[MAX_LINE] = {0};
	char file_content[MAX_LINE] = {0};
	int seq = 0;

	int n = snprintf(file_path, sizeof(file_path), "%s/%s/queue/", config->queue_path, tuid);
	log_debug("queue file path:%s", file_path);

	// 检查目录是否存在
	if ( is_dir_exits(file_path) != 0) {
		log_error("is_dir_exit fail");
		return -1;
	}

	(void)umask(033);
	srandom(time(NULL));

	for (i= 0; i< 100; i++) {
		n = snprintf(file_content, sizeof(file_content), "%s/%.5ld%.5u%.3d", file_path,  time(NULL), getpid(), seq++);
		if (n <= 0) {
			log_error("create file_content fail");
			return -1;
		}	

		if (access(file_content, F_OK) == 0 ) {
			continue;
		}

		fd = open(file_content, O_WRONLY | O_EXCL | O_CREAT, 0755);
		if (fd != -1) {
			fp = fdopen(fd, "wb");
			if(fp) {
				break;
			}
			close(fd);
		}
		log_error("open file:(%s) error:%m", file_content);
	}

	n = fwrite(content, content_len, 1, fp);
	if (n != 1) {
		log_error("fwrite to file:%s fail:%s", file_content, strerror(errno));
		
		fclose(fp);
		return -1;	
	}

	fclose(fp);
	fp = NULL;

	snprintf(file_name, file_name_size, "%s", file_content);

	return content_len;
}


int write_content_to_file_with_uid_mfile(struct config_t *config, char *tuid, char *content, size_t content_len, MFILE *mfp_in, char *file_name, size_t file_name_size)
{
	FILE *fp = NULL;
	int i = 0;
	int fd = 0;
	char file_path[MAX_LINE] = {0};
	char file_content[MAX_LINE] = {0};
	int seq = 0;

	int n = snprintf(file_path, sizeof(file_path), "%s/%s/queue/", config->queue_path, tuid);
	log_debug("queue file path:%s", file_path);

	// 检查目录是否存在
	if ( is_dir_exits(file_path) != 0) {
		log_error("is_dir_exit fail");
		return -1;
	}

	(void)umask(033);
	srandom(time(NULL));

	for (i= 0; i< 100; i++) {
		n = snprintf(file_content, sizeof(file_content), "%s/%.5ld%.5u%.3d", file_path,  time(NULL), getpid(), seq++);
		if (n <= 0) {
			log_error("create file_content fail");
			return -1;
		}	

		if (access(file_content, F_OK) == 0 ) {
			continue;
		}

/*
		//fd = open(file_content, O_WRONLY | O_EXCL | O_CREAT, 0755);
		fd = open(file_content, O_APPEND | O_EXCL | O_CREAT, 0755);
		if (fd != -1) {
			//fp = fdopen(fd, "wb");
			fp = fdopen(fd, "ab");
			if(fp) {
				break;
			}
			close(fd);
		}
		log_error("open file:(%s) error:%m", file_content);
*/
		fp = fopen(file_content, "ab");
		if (fp != NULL) {
			break;
		}
		log_error("fopen file:(%s) error:%s", file_content, strerror(errno));
	}

	log_debug("content:%d:%s", content_len, content);

	int is_need_read_again = 1;
	n = content_len;
	while ( content[n-1] == '\r' || content[n-1] == '\n' ) {
		is_need_read_again = 0;
		n--;
	}
	if (is_need_read_again == 0) {
		content[n] = '\0';	
	}

	// 先写之前取出来的数据
	n = fwrite(content, n, 1, fp);
	if (n != 1) {
		log_error("fwrite to file:%s fail:%s", file_content, strerror(errno));
		
		fclose(fp);
		return -1;	
	}

	if (is_need_read_again == 1) {
		// 再写mfile里剩下的数据
		char data[BUF_SIZE] = {0};
		while (1) {
			n = mread(mfp_in, data, sizeof(data));
			if (n == 0)
				break;
			n--;
			n = fwrite(data, n, 1, fp);
			if (n != 1)
				break;	
		}
	}

	fclose(fp);
	fp = NULL;

	snprintf(file_name, file_name_size, "%s", file_content);

	return content_len;
}



//int write_b64_content_to_file_with_uid_mfile(struct config_t *config, char *tuid, char *content, size_t content_len, MFILE *mfp_in, char *old_img_name, char *file_name, size_t file_name_size)
int write_b64_content_to_file_with_uid_mfile(char *file_path, char *content, size_t content_len, MFILE *mfp_in, char *old_img_name, char *file_name, size_t file_name_size)
{
	FILE *fp = NULL;
	int i = 0;
	int n = 0;
	int fd = 0;
	char img_name[MAX_LINE] = {0};
	char file_full_path[MAX_LINE] = {0};
	int seq = 0;

	//int n = snprintf(file_path, sizeof(file_path), "%s/%s/images/", config->queue_path, tuid);
	log_debug("save file path:%s", file_path);

	// 检查目录是否存在
	if ( is_dir_exits(file_path) != 0) {
		log_error("is_dir_exit fail");
		return -1;
	}

	(void)umask(033);
	srandom(time(NULL));

	for (i= 0; i< 100; i++) {
		if (*old_img_name && (strlen(old_img_name) > 0)) {
			n = snprintf(file_name, file_name_size, "%.5ld%.5u%.3d_%s", time(NULL), getpid(), seq++, old_img_name);
		} else {
			n = snprintf(file_name, file_name_size, "%.5ld%.5u%.3d", time(NULL), getpid(), seq++);
		}
		if (n <= 0) {
			log_error("create file_name fail");
			return -1;
		}

		n = snprintf(file_full_path, sizeof(file_full_path), "%s/%s", file_path, file_name);
		if (n <= 0) {
			log_error("create file_full_path fail");
			return -1;
		}	

		if (access(file_full_path, F_OK) == 0 ) {
			continue;
		}

		fp = fopen(file_full_path, "ab");
		if (fp != NULL) {
			break;
		}
		log_error("fopen file:(%s) error:%s", file_full_path, strerror(errno));
	}

	int is_need_read_again = 1;
	n = content_len;
	while ( content[n-1] == '\r' || content[n-1] == '\n' ) {
		is_need_read_again = 0;
		n--;
		content[n] = '\0';	
	}

/*
	content_len = strlen(content);

	log_debug("content:%d:%s", content_len, content);

	// 用于b64解码字节补齐
	char byte[5] = {0};
	memset(byte, 0, sizeof(byte));

	int byte_n = 4 - (content_len % 4);
	if (byte_n != 4) {
		int r = 0;
		n = mread(mfp_in, byte, byte_n);
		while (r < n) {
			content[content_len] = byte[r];
			content_len++;
			r++;
		}
	}


	// base64 解码
	char b64_buf[MAX_BLOCK_SIZE] = {0};
	int b64_n = MAX_BLOCK_SIZE;

	log_debug("content:%d:%s", strlen(content), content);

	b64_n = base64_decode(content, b64_buf, b64_n);
	if (b64_n <= 0) {
		log_error("base64_decode fail:%d", b64_n);
		goto WB64_FAIL;
	}
	log_debug("b64_buf:%d:%s", b64_n, b64_buf);

	// 先写之前取出来的数据
	n = fwrite(b64_buf, b64_n, 1, fp);
	if (n != 1) {
		log_error("fwrite to file:%s fail:%s", file_full_path, strerror(errno));
		
		goto WB64_FAIL;
	}

	if (is_need_read_again == 1) {
		char data[BUF_SIZE] = {0};

		while (1) {
			// 再写mfile里剩下的数据
			memset(b64_buf, 0, sizeof(b64_buf));
			memset(data, 0, sizeof(data));

			n = mread(mfp_in, data, (sizeof(data) - 1024));
			if (n == 0)
				break;
			n--;

			log_debug("mread:%d:%s", n, data);
			b64_n = base64_decode(data, b64_buf, sizeof(b64_buf));			
			if (b64_n <= 0) {
				log_error("base64_decode fail:%d", b64_n);
				goto WB64_FAIL;
			}
			log_debug("b64_buf:%d:%s", b64_n, b64_buf);

			n = fwrite(b64_buf, b64_n, 1, fp);
			if (n != 1)
				break;	
		}
	}
*/

	int b64_n = sj_b64_decode(mfp_in, content, n, fp);
	if (b64_n <= 0) {
		log_error("base64_decode fail:%d", b64_n);
		goto WB64_FAIL;
	}	



	fclose(fp);
	fp = NULL;


	return 0;


WB64_FAIL:
	if (fp) {
		fclose(fp);
		fp = NULL;
	}

	if (access(file_full_path, F_OK) == 0 ) {
		unlink(file_full_path);
    }

	return -1;


WB64_SUCC:
	if (fp) {
		fclose(fp);
		fp = NULL;
	}

	return 0;

}



int write_content_to_file_with_path(char *file_path, char *old_name, char *content, size_t content_len, char *file_name, size_t file_name_size)
{
	FILE *fp = NULL;
	int i = 0;
	int n = 0;
	int fd = 0;
	char file_full_path[MAX_LINE] = {0};
	int seq = 0;

	log_debug("queue file path:%s", file_path);

	// 检查目录是否存在
	if ( is_dir_exits(file_path) != 0) {
		log_error("is_dir_exit fail");
		return -1;
	}

	(void)umask(033);
	srandom(time(NULL));

	for (i= 0; i< 100; i++) {
		if (*old_name && (strlen(old_name) > 0)) {
			n = snprintf(file_name, file_name_size, "%.5ld%.5u%.3d_%s", time(NULL), getpid(), seq++, old_name);
		} else {
			n = snprintf(file_name, file_name_size, "%.5ld%.5u%.3d", time(NULL), getpid(), seq++);
		}
		if (n <= 0) {
			log_error("create file_name fail");
			return -1;
		}

		n = snprintf(file_full_path, sizeof(file_full_path), "%s/%s", file_path, file_name);
		if (n <= 0) {
			log_error("create file_content fail");
			return -1;
		}	

		if (access(file_full_path, F_OK) == 0 ) {
			continue;
		}

		fd = open(file_full_path, O_WRONLY | O_EXCL | O_CREAT, 0755);
		if (fd != -1) {
			fp = fdopen(fd, "wb");
			if(fp) {
				break;
			}
			close(fd);
		}
		log_error("open file:(%s) error:%m", file_full_path);

	}

	n = fwrite(content, content_len, 1, fp);
	if (n != 1) {
		log_error("fwrite to file:%s fail:%s", file_full_path, strerror(errno));
		
		fclose(fp);
		return 1;	
	}

	fclose(fp);
	fp = NULL;

	return content_len;
}

/*int has_offline_msg(struct config_t *config, char *myuid)
{
	char file_path[MAX_LINE] = {0};
    char file_content_idx[MAX_LINE] = {0};

    int n = snprintf(file_path, sizeof(file_path), "%s/%s", config->queue_path, myuid);
    n = snprintf(file_content_idx, sizeof(file_content_idx), "%s/%s", file_path, OFFLINE_MSG_CONENT_IDX);
    if (n <= 0) {
        log_error("create file_content_idx fail");
        return 1;
    }   

	if ( access(file_content_idx, F_OK) == 0 ) {
		return 1;
	}

	return 0;
}*/

// return:
// 	0: succ		1:fail
char *get_offline_msg_with_uid(struct config_t *config,char *myuid) 
{
	char file_path[MAX_LINE] = {0};
	char file_content[MAX_LINE] = {0};
	char file_content_idx[MAX_LINE] = {0};

	int n = snprintf(file_path, sizeof(file_path), "%s/%s", config->queue_path, myuid);

	n = snprintf(file_content, sizeof(file_content), "%s/%s", file_path, OFFLINE_MSG_CONTENT); 
	if (n <= 0) {
		log_error("create file_content fail");
		return NULL;
	}

	n = snprintf(file_content_idx, sizeof(file_content_idx), "%s/%s", file_path, OFFLINE_MSG_CONENT_IDX);
	if (n <= 0) {
		log_error("create file_content_idx fail");
		return NULL;
	}

	struct stat st;
	if ( stat(file_content, &st) == -1 ) {
		log_error("stat file:%s fail:%s", file_content, strerror(errno));
		
		return NULL;
	}

	char *pmsg = (char *)calloc(1, st.st_size + 10);
	if (pmsg == NULL) {
		log_error("calloc %d fail:%s", st.st_size + 10, strerror(errno));

		return NULL;
	}

	FILE *fpc = fopen(file_content, "r");	
	if (fpc == NULL) {
		log_error("fopen file:%s fail:%s", file_content, strerror(errno));
		return NULL;
	}

	n = fread(pmsg, st.st_size, 1, fpc);
	if (n != 1) {
		log_error("fread file:%s fail", file_content, strerror(errno));
		
		fclose(fpc);
		return NULL;
	}

	fclose(fpc);

	// 清除文件
	//unlink(file_content);
	//unlink(file_content_idx);

	return pmsg;
}



void ThrowWandException(MagickWand *wand)
{
	char *description;
	ExceptionType severity;

	description = MagickGetException(wand, &severity);
	log_error("%s %s %lu %s\n", GetMagickModule(), description);
	description = (char *)MagickRelinquishMemory(description);

}

int image_resize_v1(char *old_img, char *new_img, int w, int h, int compress_quality)
{
	if (*old_img == '\0') {
		return 1;
	}

	MagickBooleanType status;
	MagickWand *magick_wand;

	// init magick_wand.
	MagickWandGenesis();
	magick_wand = NewMagickWand();	

	// Read an image.
	status = MagickReadImage(magick_wand, old_img);
	if (status == MagickFalse) {
		ThrowWandException(magick_wand);
		goto FAIL;
	}

	// Get the image's width and height
	// int width = MagickGetImageWidth(magick_wand);
	// int height = MagickGetImageHeight(magick_wand);
	// fprintf(stderr, "get image w:%d h:%d\n", width, height);

	MagickResizeImage(magick_wand, w, h, LanczosFilter, 1.0);
	
	// set the compression quality to 95 (high quality = low compression)
	MagickSetImageCompressionQuality(magick_wand, compress_quality);

	// Write the image then destroy it.
	status = MagickWriteImages(magick_wand, new_img, MagickTrue);
	if (status == MagickFalse) {
		ThrowWandException(magick_wand);
		goto FAIL;
	}

	// Clean up
	if (magick_wand) {
		magick_wand = DestroyMagickWand(magick_wand);
	}
	MagickWandTerminus();

	return 0;

FAIL:
	// Clean up
	if (magick_wand) {
        magick_wand = DestroyMagickWand(magick_wand);
    }   
    MagickWandTerminus();	

	return 1;
	
}

int image_resize_without_scale(char *old_img, char *new_img, int compress_quality)
{
	if (*old_img == '\0') {
		return 1;
	}

	MagickBooleanType status;
	MagickWand *magick_wand;

	// init magick_wand.
	MagickWandGenesis();
	magick_wand = NewMagickWand();	

	// Read an image.
	status = MagickReadImage(magick_wand, old_img);
	if (status == MagickFalse) {
		ThrowWandException(magick_wand);
		goto FAIL;
	}

	// Get the image's width and height
	int width = MagickGetImageWidth(magick_wand);
	int height = MagickGetImageHeight(magick_wand);
	log_debug("get image w:%d h:%d", width, height);
	
	float new_w = 139.0;
	float new_h = 139.0;
	int w = 0;
	int h = 0;
	if ( (new_w / new_h) > (width / height) ) {
		w = new_h * width / height;
		h = new_h;	
	} else {
		w = new_w;
		h = new_w * height / width;
	}
	log_debug("get thumb w:%d h:%d", w, h);

	MagickResizeImage(magick_wand, w, h, LanczosFilter, 1.0);
	
	// set the compression quality to 95 (high quality = low compression)
	MagickSetImageCompressionQuality(magick_wand, compress_quality);

	// Write the image then destroy it.
	status = MagickWriteImages(magick_wand, new_img, MagickTrue);
	if (status == MagickFalse) {
		ThrowWandException(magick_wand);
		goto FAIL;
	}

	// Clean up
	if (magick_wand) {
		magick_wand = DestroyMagickWand(magick_wand);
	}
	MagickWandTerminus();

	return 0;

FAIL:
	// Clean up
	if (magick_wand) {
        magick_wand = DestroyMagickWand(magick_wand);
    }   
    MagickWandTerminus();	

	return 1;
	
}


// http api -----------------------------------
struct curl_return_string {
    char *str;
    size_t len;
    size_t size;
}; // 用于存curl返回的结果

size_t _recive_data_from_http_api(void *buffer, size_t size, size_t nmemb, void *user_p)
{
    struct curl_return_string *result_t = (struct curl_return_string *)user_p;

    if (result_t->size < ((size * nmemb) + 1)) {
        result_t->str = (char *)realloc(result_t->str, (size * nmemb) + 1);
        if (result_t->str == NULL) {
            return 0;
        }
        result_t->size = (size * nmemb) + 1;
    }

    result_t->len = size * nmemb;
    memcpy(result_t->str, buffer, result_t->len);
    result_t->str[result_t->len] = '\0';

    return result_t->len;
}

void clean_mem_buf(char *buf)
{
	if (buf != NULL) {
		free(buf);
		buf = NULL;
	}
}


char *login_http_api(char *url, char *account, char *password, int get_friend_list, int *result_len)
{
	// 声明保存返回 http 的结果
	struct curl_return_string curl_result_t;

	char *presult = NULL;
	*result_len = 0;

	curl_result_t.len = 0;
    curl_result_t.str = (char *)calloc(10, 1024);
	if (curl_result_t.str == NULL) {
		log_error("calloc fail for curl_result_t.str");
		return NULL;
	}	
	curl_result_t.size = 10 * 1024;

	curl_global_init(CURL_GLOBAL_ALL);

	CURL *curl;
	CURLcode ret;

	// init curl
	curl = curl_easy_init();
	if (!curl) {
		log_error("couldn't init curl");
		goto FAIL;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POST, 1);    // use post 

	// urlencode post data
	char *account_encode = curl_easy_escape(curl, account, strlen(account));
	if (!account_encode) {
		log_error("urlencode account fail, so use source data");
		account_encode = account;
	}

	char *password_encode = curl_easy_escape(curl, password, strlen(password));
	if (!password_encode) {
		log_error("urlencode password fail, so use source data");
		password_encode = password;
	}

	//char *ios_token_encode = curl_easy_escape(curl, ios_token, strlen(ios_token));
	//if (!ios_token_encode) {
		//log_error("urlencode ios_token_encode fail, so use source data");
		//ios_token_encode = ios_token;
	//}

	int request_data_len = 8 + strlen(account_encode) + 10 +strlen(password_encode) + 11 + 50;
	char *request_data = (char *)calloc(1, request_data_len) ;
	if (request_data == NULL) {
		log_error("calloc fail for request_data");
		curl_easy_cleanup(curl);
		curl_global_cleanup();

		goto FAIL;
	}

	//snprintf(request_data, request_data_len, "account=%s&password=%s&ios_token=%s&get_friend_list=%d", account_encode, password_encode, ios_token_encode, get_friend_list);
	snprintf(request_data, request_data_len, "account=%s&password=%s&get_friend_list=%d", account_encode, password_encode, get_friend_list);
	log_debug("request data:%s\n", request_data);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_data);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _recive_data_from_http_api);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curl_result_t);  // 传这个参数过去

	ret = curl_easy_perform(curl);
	if(ret != CURLE_OK) {
		log_error("curl_easy_perform() failed:%s, url:%s", curl_easy_strerror(ret), url);

		curl_easy_cleanup(curl);
		curl_global_cleanup();
	} else {
		if (curl_result_t.str) {
			log_info("request url:%s response:[%d]%s", url, curl_result_t.len, curl_result_t.str);

			// return is json data
			presult = (char *)calloc(1, curl_result_t.len + 1);
			if (presult == NULL) {
				log_error("calloc presult fail:%s", strerror(errno));
				goto FAIL;
			}		
			*result_len = snprintf(presult, curl_result_t.len + 1, "%s", curl_result_t.str);

			goto SUCC;
		}
	}



FAIL:
	clean_mem_buf(curl_result_t.str);
	curl_result_t.len = 0;
	curl_result_t.size = 0;

	clean_mem_buf(account_encode);
	clean_mem_buf(password_encode);
	//clean_mem_buf(ios_token_encode);

	clean_mem_buf(request_data);

	return NULL;


SUCC:
	clean_mem_buf(curl_result_t.str);
	curl_result_t.len = 0;
	curl_result_t.size = 0;

	clean_mem_buf(account_encode);
	clean_mem_buf(password_encode);
	//clean_mem_buf(ios_token_encode);

	clean_mem_buf(request_data);

	return presult;
}


int write_queue_to_db(struct config_t *config, char *tag_type, char *cid, char *fuid, char *fnick, char *fios_token, char *tuid, char *tios_token, char *queue_type, char *queue_file, int queue_size)
{
	return 1;
}

// Return:
//	-1: error
//	other: mysql id
int write_queue_to_db_v2(struct config_t *config, char *mid, char *tag_type, char *fuid, char *fnick, char *tuid, char *queue_type, char *queue_file, long long queue_size, char *image_wh)
{
    log_debug("quar_mysql:%s mysql_user:%s mysql_pass:%s mysql_db:%s mysql_port:%d", config->mysql_host, config->mysql_user, config->mysql_passwd, config->mysql_db, config->mysql_port);

    MYSQL mysql, *mysql_sock;
    MYSQL_RES* res = NULL;
    char sql[5 * MAX_LINE] = {0};
    
    mysql_init(&mysql);
    if (!(mysql_sock = mysql_real_connect(&mysql, config->mysql_host, config->mysql_user, config->mysql_passwd, config->mysql_db, config->mysql_port, NULL, 0))) {
        log_error("connect mysql fail");
        return -1;
    }   
    
    // addslash
    char mysql_tag_type[MAX_LINE] = {0}; 
    char mysql_fnick[MAX_LINE] = {0}; 
    char mysql_queue_type[MAX_LINE] = {0}; 
    char mysql_queue_file[MAX_LINE] = {0}; 
    char mysql_image_wh[MAX_LINE] = {0};

    mysql_real_escape_string(mysql_sock, mysql_tag_type, tag_type, strlen(tag_type));
    mysql_real_escape_string(mysql_sock, mysql_fnick, fnick, strlen(fnick));
    mysql_real_escape_string(mysql_sock, mysql_queue_type, queue_type, strlen(queue_type));
    mysql_real_escape_string(mysql_sock, mysql_queue_file, queue_file, strlen(queue_file));
    mysql_real_escape_string(mysql_sock, mysql_image_wh, image_wh, strlen(image_wh));

	// get current time
	/*struct tm when;
	time_t now_time;
	time(&now_time);
	when = *localtime(&now_time);
	char cdate[21] = {0};
	snprintf(cdate, sizeof(cdate), "%04d-%02d-%02d %02d:%02d:%02d\n", when.tm_year + 1900, when.tm_mon + 1, when.tm_mday, when.tm_hour, when.tm_min, when.tm_sec); 
    
    snprintf(sql, sizeof(sql), "insert into liao_queue (tag_type, mid, fuid, fnick, fios_token, tuid, tios_token, queue_type, queue_file, queue_size, cdate, expire) "
                                                    "values ('%s', '%s', %d, '%s', '%s', %d, '%s', '%s', '%s', %d, '%s', 0);",
                                                    mysql_tag_type, cid, atoi(fuid), mysql_fnick, mysql_fios_token, atoi(tuid), mysql_tios_token, mysql_queue_type, mysql_queue_file, queue_size, cdate); */
                                                    
    snprintf(sql, sizeof(sql), "INSERT INTO sc_queue (mid, tag_type, fuid, fnick, tuid, queue_type, queue_file, queue_size, image_wh, expire) "
    										"VALUES ('%s', '%s', %s, '%s', %s, '%s', '%s', %lld, '%s', 0)", 
    										mid, mysql_tag_type, fuid, mysql_fnick, tuid, mysql_queue_type, mysql_queue_file, queue_size, 
    										mysql_image_wh);
    
    log_debug("sql:%s", sql);
    
    if (mysql_query(mysql_sock, sql)) {
        log_error("insert mysql liao_queue fail:%s", mysql_error(mysql_sock));
        mysql_close(mysql_sock);
    
        return -1;
    }   

	int qid = mysql_insert_id(mysql_sock);
    
    mysql_close(mysql_sock);
    
    return qid;
}


// type: 0:login	1:logout
int user_login(struct config_t *config, int type, char *uid, char *ios_token)
{
    log_debug("quar_mysql:%s mysql_user:%s mysql_pass:%s mysql_db:%s mysql_port:%d", config->mysql_host, config->mysql_user, config->mysql_passwd, config->mysql_db, config->mysql_port);

    MYSQL mysql, *mysql_sock;
    MYSQL_RES* res = NULL;
    char sql[MAX_LINE] = {0};
    
    mysql_init(&mysql);
    if (!(mysql_sock = mysql_real_connect(&mysql, config->mysql_host, config->mysql_user, config->mysql_passwd, config->mysql_db, config->mysql_port, NULL, 0))) {
        log_error("connect mysql fail");
        return -1;
    }   
    
    // addslash
    char mysql_ios_token[MAX_LINE] = {0}; 

    mysql_real_escape_string(mysql_sock, mysql_ios_token, ios_token, strlen(ios_token));


	// get current time
	/*struct tm when;
	time_t now_time;
	time(&now_time);
	when = *localtime(&now_time);
	char cdate[21] = {0};
	snprintf(cdate, sizeof(cdate), "%04d-%02d-%02d %02d:%02d:%02d\n", when.tm_year + 1900, when.tm_mon + 1, when.tm_mday, when.tm_hour, when.tm_min, when.tm_sec);*/
    
	if (type == 0) {
		// get old ios_token
		MYSQL_ROW   row;
		snprintf(sql, sizeof(sql), "select ios_token from liao_logined where uid = %d limit 1", atoi(uid));
		if (mysql_query(mysql_sock, sql)) {
			log_error("exec %s mysql liao_logined fail:%s", sql, mysql_error(mysql_sock));
			mysql_close(mysql_sock);

			return -1;
		}	

		if (!(res = mysql_store_result(mysql_sock))) {
			log_error("exec %s could not get result fail:%s", sql, mysql_error(mysql_sock));
    		mysql_close(mysql_sock);
			return -1;
		}

		if (row = mysql_fetch_row(res)) {
			if ( strcasecmp(row[0], ios_token) == 0 ) {
				mysql_free_result(res);	
    			mysql_close(mysql_sock);
				return 0;

			} else {
				// ios_token changed
				snprintf(sql, sizeof(sql), "update liao_logined set ios_token='%s' where uid = %d limit 1", mysql_ios_token, atoi(uid));
			}

		} else {
			// insert logined
    		snprintf(sql, sizeof(sql), "insert into liao_logined (uid, ios_token) "
                                                    "values (%d, '%s');",
                                                    atoi(uid), mysql_ios_token); 
		}

		mysql_free_result(res);
	} else {
		//snprintf(sql, sizeof(sql), "delete from liao_logined where uid = %d and ios_token = '%s' limit 1", atoi(uid), mysql_ios_token);
		snprintf(sql, sizeof(sql), "delete from liao_logined where uid = %d limit 1", atoi(uid));
	}
	
    log_debug("sql:%s", sql);
    
    if (mysql_query(mysql_sock, sql)) {
        log_error("exec %s mysql liao_logined fail:%s", sql, mysql_error(mysql_sock));
        mysql_close(mysql_sock);
    
        return -1;
    }   

    mysql_close(mysql_sock);
    
    return 0;
}

// 返回:
//	-1: 出错
//	 0: 用户登出
//	>0: 用户登录中
int get_iostoken_logined(struct config_t *config, char *tuid, char *tios_token, size_t tios_token_size)
{
    log_debug("quar_mysql:%s mysql_user:%s mysql_pass:%s mysql_db:%s mysql_port:%d", config->mysql_host, config->mysql_user, config->mysql_passwd, config->mysql_db, config->mysql_port);

    MYSQL mysql, *mysql_sock;
    MYSQL_RES* res = NULL;
	MYSQL_FIELD *mfd = NULL;
	MYSQL_ROW	row;
	
	int n = 0;
    char sql[MAX_LINE] = {0};
    
    mysql_init(&mysql);
    if (!(mysql_sock = mysql_real_connect(&mysql, config->mysql_host, config->mysql_user, config->mysql_passwd, config->mysql_db, config->mysql_port, NULL, 0))) {
        log_error("connect mysql fail");
        return -1;
    }   
    

	// get current time
	/*struct tm when;
	time_t now_time;
	time(&now_time);
	when = *localtime(&now_time);
	char cdate[21] = {0};
	snprintf(cdate, sizeof(cdate), "%04d-%02d-%02d %02d:%02d:%02d\n", when.tm_year + 1900, when.tm_mon + 1, when.tm_mday, when.tm_hour, when.tm_min, when.tm_sec);*/
    
	snprintf(sql, sizeof(sql), "select * from sc_logined where uid = %d limit 1", atoi(tuid));
	
    log_debug("sql:%s", sql);
    
    if (mysql_query(mysql_sock, sql)) {
        log_error("exec %s mysql liao_logined fail:%s", sql, mysql_error(mysql_sock));
        mysql_close(mysql_sock);
    
        return -1;
    }   

	if (!(res = mysql_store_result(mysql_sock))) {
		mysql_close(mysql_sock);
		log_error("could not get result '%s' from %s", sql, mysql_error(mysql_sock));
		
		return -1;
	}

	if (row = mysql_fetch_row(res)) {
		n = snprintf(tios_token, tios_token_size, "%s", row[2]);
	}

	mysql_free_result(res);
    mysql_close(mysql_sock);
    
    return n;
}

void mybyte_copy( register char *to, register unsigned int n, register char *from)
{
    for (;;) 
    {    
        if (!n) 
            return;
        *to++ = *from++;
        --n; 
        if (!n) 
            return;
        *to++ = *from++;
        --n; 
        if (!n) 
            return;
        *to++ = *from++;
        --n; 
        if (!n) 
            return;
        *to++ = *from++;
        --n; 
    }    
}

int fast_write(MFILE *mfp, char *buf, size_t buf_len)
{
	int wr = mwrite(mfp, buf, buf_len);
	if (wr != 1) {
		mclose(mfp);
		mfp = NULL;	
		return 0;
	}
	return wr;
}


int fd_copy( int to, int from)
{
    if (to == from)
        return 0;
    if (fcntl(from, F_GETFL, 0) == -1) 
        return -1; 
    close(to);
    if (fcntl(from, F_DUPFD, to) == -1) 
        return -1; 
    return 0;
}

int fd_move( int to, int from)
{
    if (to == from)
        return 0;
    if (fd_copy(to, from) == -1) 
        return -1; 
    close(from);
    return 0;
}



void send_to_socket(int sockfd, char *rspsoneBuf, int responseBuf_len)
{
    int nwrite = 0;
    //int buf_len = strlen(rspsoneBuf);
    int buf_len = responseBuf_len;
    int n = buf_len;
    while (n > 0) {
        nwrite = write(sockfd, rspsoneBuf + buf_len - n, n);
        if (nwrite < n) {
            if (nwrite == -1 && errno != EAGAIN) {
                log_error("write to fd[%d] failed:[%d]%s", sockfd, errno, strerror(errno));
            }
            break;
        }

        n -= nwrite;
    }  

    log_debug("send fd[%d]:%s", sockfd, rspsoneBuf);
}

void send_to_websocket(int sockfd, char *websocket_data, int websocket_data_len)
{
	char *rspsoneBuf = NULL;
	unsigned long responseBuf_len = 0;
	rspsoneBuf = websocket_fmt_data_encode(websocket_data, websocket_data_len, &responseBuf_len);
	if (rspsoneBuf == NULL || responseBuf_len <= 0) {
		log_error("websocket_fmt_data_encode fail, do not send data to web client");
		return ;
	}

    int nwrite = 0;
    //int buf_len = strlen(rspsoneBuf);
    int buf_len = responseBuf_len;
    int n = buf_len;
    while (n > 0) {
        nwrite = write(sockfd, rspsoneBuf + buf_len - n, n);
        if (nwrite < n) {
            if (nwrite == -1 && errno != EAGAIN) {
                log_error("write to fd[%d] failed:[%d]%s", sockfd, errno, strerror(errno));
            }
            break;
        }

        n -= nwrite;
    }  

    log_debug("send fd[%d]:%s", sockfd, rspsoneBuf);
    
    if (rspsoneBuf) {
    	free(rspsoneBuf);
    	rspsoneBuf = NULL;
    	responseBuf_len = 0;
    }
}


int sha1_encode(char *src, size_t src_len, char *dst)
{
	//SHA1(src, src_len - 1, dst);
	SHA_CTX stx;
	
	SHA1_Init(&stx);
	SHA1_Update(&stx, src, src_len);
	SHA1_Final(dst, &stx);

	return strlen(dst);
}


// return:
//	0:succ
//	1:未结束
//	2:关闭
//	3:错误
//	4:ping
int websocket_fmt_data_decode(char *buf, size_t buf_size, char *payload_data, size_t payload_data_size)
{
	/*
	具体每一bit的意思
	FIN      1bit 表示信息的最后一帧
	RSV 1-3  1bit each 以后备用的 默认都为 0
	Opcode   4bit 帧类型，稍后细说
	Mask     1bit 掩码，是否加密数据，默认必须置为1
	Payload  7bit 数据的长度
	Masking-key      1 or 4 bit 掩码
	Payload data     (x + y) bytes 数据
	Extension data   x bytes  扩展数据
	Application data y bytes  程序数据
	
	OPCODE：4位
	解释PayloadData，如果接收到未知的opcode，接收端必须关闭连接。
	0x0表示附加数据帧
	0x1表示文本数据帧
	0x2表示二进制数据帧
	0x3-7暂时无定义，为以后的非控制帧保留
	0x8表示连接关闭
	0x9表示ping
	0xA表示pong
	0xB-F暂时无定义，为以后的控制帧保留
*/

	char fin;
	char optcode;
	char mask_flag;
	char masks[4];
	//char payload_data[MAX_BLOCK_SIZE];
	char temp[8];
	unsigned long n, payload_len = 0;
	unsigned short us_leng = 0;
	int i = 0;
	
	n = buf_size;
	
	if (n < 2) {
		return 3;	
	}
						
	fin = (buf[0] & 0x80) == 0x80;	// 1bit, 1表示最后一帧
	log_debug("fin:%d", fin);
	if (!fin) {
		// 超过一帧暂不处理 
		return 3;
	}
						
	optcode = (buf[0] & 0xF);
	log_debug("optcode:%x", optcode);
	if (optcode == 0x8) {
		// 关闭连接
		log_debug("WebSocket client close");
		return 2;
		
	} else if (optcode == 0x9) {
		// ping
		log_debug("WebSocket client ping");
		return 4;
		
	}
	
	mask_flag = (buf[1] & 0x80) == 0x80; // 是否包含掩码 
	log_debug("mask_flag:%x", mask_flag);
	if (!mask_flag) {
		// 不包含掩码的暂不处理
		return 3;
	}
	
	payload_len = buf[1] & 0x7F; // 数据长度
	log_debug("payload_len:%d", payload_len);
	if (payload_len == 126) {
		memcpy(masks, buf+4, 4);
		payload_len = (buf[2] & 0xFF) << 8 | (buf[3] & 0xFF);
			
		memset(payload_data, 0, payload_data_size);
		memcpy(payload_data, buf + 8, payload_len);
		
	} else if (payload_len == 127) {
		memcpy(masks, buf+10, 4);
		for (i=0; i<8; i++) {
			temp[i] = buf[9 - i];
		}
		memcpy(&n, temp, 8);
		payload_len = n;
		
		memset(payload_data, 0, payload_data_size);
		memcpy(payload_data, buf + 14, payload_len);

	} else {
		memcpy(masks, buf+2, 4);
		
		memset(payload_data, 0, payload_data_size);
		memcpy(payload_data, buf + 6, payload_len);
	
	}
	
	for (i=0; i<payload_len; i++) {
		payload_data[i] = (char)(payload_data[i] ^ masks[i % 4]);
	}
	
	return 0;
}

// return:
//	0: fail 
//	>0:	succ
char *websocket_fmt_data_encode(char *buf, size_t buf_size, unsigned long *send_data_len)
{
	char *send_data = NULL;
	unsigned long n = buf_size;
	if (n < 126) {
		send_data = (char *)calloc(1, n + 2);
		if (send_data == NULL) {
			log_error("calloc fail");
			return NULL;
		}
		send_data[0] = 0x81;
		send_data[1] = n;
		memcpy(send_data + 2, buf, n);
		*send_data_len = n + 2;
		
	} else if (n < 0xFFFF) {
		send_data = (char *)calloc(1, n + 4);
		if (send_data == NULL) {
			log_error("calloc fail");
			return NULL;
		}
		
		send_data[0] = 0x81;
		send_data[1] = 126;
		send_data[2] = (n >> 8 & 0xFF);
		send_data[3] = (n & 0xFF);
		memcpy(send_data + 4, buf, n);
		*send_data_len = n + 4;
	
	} else {
		// 暂不处理超长内容
		*send_data_len = 0;
	}
	
	return send_data;
	
}
