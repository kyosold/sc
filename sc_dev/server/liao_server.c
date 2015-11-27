/*
*
*	command:
*		<命令> <内容>
*
*		命令:
*			HELO: 用于表示客户端自己的信息		helo uid:0000010 ios_token:xxxxxxxxxxxx
*
*
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>  
#include <sys/epoll.h>  
#include <sys/sendfile.h>  
#include <sys/wait.h>  
#include <netinet/in.h>  
#include <netinet/tcp.h>  
#include <arpa/inet.h>  
#include <strings.h>  

#include "confparser.h"
#include "dictionary.h"

#include "liao_log.h"
//#include "liao_utils.h"
#include "liao_server.h"

#define CHILD_PROG		"/data1/htdocs/sc_dev/server/liao_command"
#define CFG_FILE		"./liao_config.ini"

dictionary *dict_conf = NULL;


//struct clients *client_st;
struct childs *child_st;
dictionary *online_d;
struct confg_st config_st;


// epoll
int epoll_fd = -1; 
int epoll_nfds = -1; 
int epoll_event_num = 0;
struct epoll_event *epoll_evts = NULL;
int epoll_num_running = 0;
int listenfd = -1;

extern int error_temp();


// ----------------
dictionary *liao_init_cond(char *conf_file);


// 配置文件操作
dictionary *liao_init_cond(char *conf_file)
{
   dictionary *dict = open_conf_file(conf_file);
   if (dict == NULL) {
       //log_error("errror");
       return NULL;
   }
    
   return dict;
}





// 处理配置文件
// return:
// 	0:succ	1:fail
int conf_init(char *cfg_file)
{
	memset(config_st.bind_port, 0, sizeof(config_st.bind_port));
	memset(config_st.log_level, 0, sizeof(config_st.log_level));
	memset(config_st.max_works, 0, sizeof(config_st.max_works));
	memset(config_st.child_cf, 0, sizeof(config_st.child_cf));

	// 检查配置文件是否正确
	if (*cfg_file == '\0') {
		fprintf(stderr, "config file is null\n");
		log_error("config file is null");
		return 1;
	}
	
	struct stat cfg_st;
	if (stat(cfg_file, &cfg_st) == -1) {
		fprintf(stderr, "file '%s' error:%s\n", cfg_file, strerror(errno));
		log_error("file '%s' error:%s", cfg_file, strerror(errno));
		return 1;
	}
	
	// 读取配置文件 
	dict_conf = liao_init_cond(cfg_file);
	if (dict_conf == NULL) {
        fprintf(stderr, "liao_init_cond fail\n");
        log_error("liao_init_cond fail");
        return 1;
    }   

	char *pbind_port = dictionary_get(dict_conf, "global:bind_port", NULL);
	if (pbind_port != NULL) {
		snprintf(config_st.bind_port, sizeof(config_st.bind_port), "%s", pbind_port);
	} else {
		log_warning("parse config bind_port error, use defaut:%s", DEF_BINDPORT);
		snprintf(config_st.bind_port, sizeof(config_st.bind_port), "%s", DEF_BINDPORT);
	}

    char *p_log_level = dictionary_get(dict_conf, "global:log_level", NULL);
    if (p_log_level != NULL) {
        if (strcasecmp(p_log_level, "debug") == 0) {
            log_level = debug;
        } else if (strcasecmp(p_log_level, "info") == 0) {
            log_level = info;
        }   
		snprintf(config_st.log_level, sizeof(config_st.log_level), "%s", p_log_level);
    } else {
		log_warning("parse config log_level error, use defaut:%s", DEF_LOGLEVEL);
		snprintf(config_st.log_level, sizeof(config_st.log_level), "%s", DEF_LOGLEVEL);
	}

	char *pmax_works = dictionary_get(dict_conf, "global:max_works", NULL);
	if (pmax_works != NULL) {
		snprintf(config_st.max_works, sizeof(config_st.max_works), "%s", pmax_works);
	} else {
		log_warning("parse config max_works error, use default:%s", DEF_MAXWORKS);
		snprintf(config_st.max_works, sizeof(config_st.max_works), "%s", DEF_MAXWORKS);
	}

	char *pchild_cf = dictionary_get(dict_conf, "global:child_cf", NULL);
	if (pchild_cf != NULL) {
		snprintf(config_st.child_cf, sizeof(config_st.child_cf), "%s", pchild_cf);
	} else {
		log_warning("parse config child_cf error, use default:%s", DEF_CHILDCF);
		snprintf(config_st.child_cf, sizeof(config_st.child_cf), "%s", DEF_CHILDCF);
	}


	return 0;
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

int epoll_add_evt(int epoll_fd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		log_error("%s add event to epoll fail:%d %s fd:%d event:%d", msg_id, errno, strerror(errno), fd, state);
		return 1;
	}
	log_debug("%s add event to epoll succ. fd:%d event:%d", msg_id, fd, state);

	epoll_num_running++;

	return 0;
}

// return: 0:succ 1:fail
int epoll_event_mod(int sockfd, int type)
{
	struct epoll_event ev;
	ev.data.fd = sockfd;
	ev.events = type;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &ev) == -1) {
    	log_error("epoll_ctl fd[%d] EPOLL_CTL_MOD EPOLLIN failed:[%d]%s", sockfd, errno, strerror(errno));
		return 1;
	} 
	return 0;
}

// return: 0:succ 1:fail
int epoll_delete_evt(int epoll_fd, int fd)
{
	struct epoll_event ev;
	ev.data.fd = fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0) == -1) {
		log_error("%s delete event from epoll fail:%d %s fd:%d", msg_id, errno, strerror(errno), fd);
		return 1;
	}
	log_debug("%s delete event from epoll succ. fd:%d", msg_id, fd);
	
	epoll_num_running--;

	return 0;
}

// 设置fd为非阻塞模式
int setnonblocking(int fd)
{
	int opts;
	opts = fcntl(fd, F_GETFL);
	if (opts < 0) {
		log_error("fcntl F_GETFL failed:[%d]:%s", errno, strerror(errno));
		return 1;
	}

	opts = (opts | O_NONBLOCK);
	if (fcntl(fd, F_SETFL, opts) < 0) {
		log_error("fcntl F_SETFL failed:[%d]:%s", errno, strerror(errno));
		return 1;
	}

	return 0;
}


// 0:succ	1:fail
int resizebuf(char *sbuf, int new_size)
{
    char *new_pbuf = (char *)realloc(sbuf, new_size);
    if (new_pbuf == NULL) {
        log_error("realloc failed:%s", strerror(errno));
            
		return 1;
    } else {
        sbuf = new_pbuf; 

		return 0;
    }  
}



void usage(char *prog)
{
	printf("Usage:\n");
	printf("%s -c[config file]\n", prog);
}


void clean_child_st(int i)
{
	if (child_st[i].client_fd != -1) {
		close(child_st[i].client_fd);
		child_st[i].client_fd = -1;
	}
	if (child_st[i].pipe_r_in != -1) {
        close(child_st[i].pipe_r_in);
        child_st[i].pipe_r_in = -1;
    } 
	if (child_st[i].pipe_r_out != -1) {
        close(child_st[i].pipe_r_out);
        child_st[i].pipe_r_out = -1;
    } 
	if (child_st[i].pipe_w_out != -1) {
        close(child_st[i].pipe_w_out);
        child_st[i].pipe_w_out = -1;
    } 
	if (child_st[i].pipe_w_in != -1) {
        close(child_st[i].pipe_w_in);
        child_st[i].pipe_w_in = -1;
    } 
					
	child_st[i].pid = -1;
	child_st[i].used = 0;
	memset(child_st[i].mid, 0, sizeof(child_st[i].mid));

	if (child_st[i].mfp_out != NULL) {
		mclose(child_st[i].mfp_out);
		child_st[i].mfp_out = NULL;
	}
}

void sig_catch(int sig, void (*f) () ) 
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
}

void sig_pipeignore()
{
	sig_catch(SIGPIPE, SIG_IGN);
}

void sig_childblock()
{
	sig_block(SIGCHLD);
}

void sig_childunblock()
{
	sig_unblock(SIGCHLD);
}

	
void sigchld()
{
	int wstat;
	int pid;
	int i;
	while ((pid = waitpid(-1, &wstat, WNOHANG)) > 0) {
		for (i = 0; i<atoi(config_st.max_works) + 1; ++i) {
			if (child_st[i].used) {
				if (child_st[i].pid == pid) {
					//log_debug("catch child exit, idx[%d]", i);			
					clean_child_st(i);
				}
			}	
		}
	}
}


void exit_self(int s) 
{
	log_debug("get sigint for exit self");

	if (epoll_fd != -1) {
		close(epoll_fd);
		epoll_fd = -1;
	}
	if (listenfd != -1) {
		close(listenfd);
		listenfd = -1;
	}
}

// 获取一个新的未使用的在client_st列表中的索引
// return: 
//  -1 为未得到(可能满了)
int new_idx_from_child_st()
{
    int idx = -1;
    int i = 0; 
    for (i=0; i<(atoi(config_st.max_works) + 1); i++) {
        if (child_st[i].used == 0) { 
            idx = i; 
            break;
        }    
    }    

    return idx; 
}


int get_idx_with_sockfd(int sockfd)
{
    int idx = -1;
    int i = 0; 
    for (i=0; i<(atoi(config_st.max_works) + 1); i++) {
        //if (child_st[i].fd_in == sockfd && child_st[i].used == 1) { 
        if ((child_st[i].pipe_r_in == sockfd) && (child_st[i].used == 1)) { 
            idx = i; 
            break;
        }    
    }    

    return idx; 
}

int get_idx_with_pid(int pid)
{
	int idx = -1;
	int i = 0;
	for (i=0; i<(atoi(config_st.max_works) + 1); i++) {
		log_debug("pid:%d i:%d pid:%d pipe_r_in:%d pipe_r_out:%d pipe_w_in:%d pipe_w_out:%d", pid, i, child_st[i].pid, child_st[i].pipe_r_in, child_st[i].pipe_r_out, child_st[i].pipe_w_in, child_st[i].pipe_w_out);
		if ( (child_st[i].pid == pid) && (child_st[i].used == 1) ) {
			idx = i;
			break;
		}
	}

	log_debug("get idx:%d", idx);
	
	return idx;
}

void send_apn_cmd(char *ios_token, char *fuid, char *fnick, char *type, char *tuid)
{
    char cmd[MAX_LINE] = {0}; 
    snprintf(cmd, sizeof(cmd), "./push.php '%s' '%s' '%s' '%s' '%s'", ios_token, fuid, fnick, type, tuid);
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


void send_to_socket(int sockfd, char *rspsoneBuf, int responseBuf_len)
{
    int nwrite = 0;
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


// type:SSYSTEM_SENDMSG pid:%s fuid:%s fnick:%s mtype:%s tuid:%s \n
void process_system(struct childs *child)
{				
	char cmd_type[MAX_LINE] = {0};
	char cmd_pid[MAX_LINE] = {0};
	char cmd_fuid[MAX_LINE] = {0};
	char cmd_fnick[MAX_LINE] = {0};
	char cmd_mtype[MAX_LINE] = {0};
	char cmd_tuid[MAX_LINE] = {0};
				
	int r = 0;		
	int buf_len = 0;
	char buf[BUF_SIZE] = {0};
	char *pbuf = buf;
	int n = mread(child->mfp_out, buf, sizeof(buf));
	buf_len = n;
	while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {
	
		char *psed = (char *)memchr(pbuf, ' ', n);
		if (psed != NULL) {
			*psed = '\0';
			int m = (psed - pbuf);
			if (m > 0) {
				// 分段取变量
				log_debug("ptok:[%s]", pbuf);
				
				if (strncasecmp(pbuf, "type:", 5) == 0) {
					r = snprintf(cmd_type, sizeof(cmd_type), "%s", pbuf+5);
					log_debug("from cmd_type:%d:%s", r, cmd_type);
					
				} else if (strncasecmp(pbuf, "pid:", 4) == 0) {
					r = snprintf(cmd_pid, sizeof(cmd_pid), "%s", pbuf+4);
					log_debug("from cmd_pid:%d:%s", r, cmd_pid);
					
				} else if (strncasecmp(pbuf, "fuid:", 5) == 0) {
					r = snprintf(cmd_fuid, sizeof(cmd_fuid), "%s", pbuf+5);
					log_debug("from cmd_fuid:%d:%s", r, cmd_fuid);
					
				} else if (strncasecmp(pbuf, "fnick:", 6) == 0) {
					r = snprintf(cmd_fnick, sizeof(cmd_fnick), "%s", pbuf+6);
					log_debug("from cmd_fnick:%d:%s", r, cmd_fnick);
					
				} else if (strncasecmp(pbuf, "mtype:", 6) == 0) {
					r = snprintf(cmd_mtype, sizeof(cmd_mtype), "%s", pbuf+6);
					log_debug("from cmd_mtype:%d:%s", r, cmd_mtype);
					
				} else if (strncasecmp(pbuf, "tuid:", 5) == 0) {
					r = snprintf(cmd_tuid, sizeof(cmd_tuid), "%s", pbuf+5);
					log_debug("from cmd_tuid:%d:%s", r, cmd_tuid);
					
				}
			}
			
			*psed = ' ';
			
			pbuf = psed + 1;
			n -= (m + 1);
		
		} else {
			if (pbuf != NULL) {
                if (strncasecmp(pbuf, "tuid:", 5) == 0) { 
                    r = snprintf(cmd_tuid, sizeof(cmd_tuid), "%s", pbuf+5);
                    while ( cmd_tuid[r-1] == '\r' || cmd_tuid[r-1] == '\n' )
                        r--; 
                    cmd_tuid[r] = '\0';
                    log_debug("from cmd_tuid:%d:%s", r, cmd_tuid);
                }    
            } 

			break;
		}
		
	}

	log_debug("system msg send child:%d -> child:%d", child->pid, atoi(cmd_pid));
	
	if (strcasecmp(cmd_type, "SSYSTEM_SENDMSG") == 0) {
		int ci = get_idx_with_pid(atoi(cmd_pid));
		if (ci == -1) {
			log_error("receiver offline, use APNS");
			// ......
			//send_apn_cmd(cmd_tios_token, cmd_fuid, cmd_fnick, cmd_mtype, cmd_tuid);
			//send_apn_cmd(cmd_fuid, cmd_fnick, cmd_mtype, cmd_tuid);
			// 用户突然离线了,由原子进程(发件人进程)负责使用apn通知收件人有新消息.s
			
			char buf_data[MAX_LINE] = {0};
			// type:SSYSTEM_SENDAPNS fuid:18 fnick:5a6L5YGl mtype:sys tuid:19
			int n = snprintf(buf_data, sizeof(buf_data), "type:SSYSTEM_SENDAPNS fuid:%s fnick:%s mtype:%s tuid:%s \n", cmd_fuid, cmd_fnick, cmd_mtype, cmd_tuid);
			send_to_socket(child->pipe_w_out, buf_data, n);
			
			return;
		}
		log_debug("get idx:%d with pid:%d, pipe_w_out:%d", ci, atoi(cmd_pid), child_st[ci].pipe_w_out);
		
		send_to_socket(child_st[ci].pipe_w_out, buf, buf_len);
		return;
	}
}


int main(int argc, char **argv)
{
	liao_log("liao_server", LOG_PID|LOG_NDELAY, LOG_MAIL);

	if (argc != 3) {
		usage(argv[0]);
		exit(0);
	}

	if (chdir(LIAO_HOME) == -1) {
		log_alert("cannot start: unable to switch to home directory");	
		exit(0);
	}

	snprintf(msg_id, sizeof(msg_id), "00000000");
	
	char cfg_file[MAX_LINE] = CFG_FILE;
	
	int c;
	const char *args = "c:h";
	while ((c = getopt(argc, argv, args)) != -1) {
		switch (c) {
			case 'c':
				snprintf(cfg_file, sizeof(cfg_file), "%s", optarg);
				break;
				
			case 'h':
			default:
				usage(argv[0]);
				exit(0);
		}
	}

	// 处理配置文件 
	if (conf_init(cfg_file)) {
		fprintf(stderr, "Unable to read control files:%s\n", cfg_file);
		log_error("Unable to read control files:%s", cfg_file);
		exit(1);
	}

	/*	
	// 检查队列目录
	if (strlen(config_st.queue_path) <= 0) {
		fprintf(stderr, "Unable to read queue path.\n");
		log_error("Unable to read queue path.");
		exit(1);
	}
	if ( access(config_st.queue_path, F_OK) ) {
		fprintf(stderr, "Queue path:%s not exists:%s, so create it.\n", config_st.queue_path, strerror(errno));	
		log_error("Queue path:%s not exists:%s, so create it.", config_st.queue_path, strerror(errno));	

		umask(0);
		mkdir(config_st.queue_path, 0777);
	}*/
	

	online_d = dictionary_new(atoi(config_st.max_works) + 1);
	

	child_st = (struct childs *)malloc((atoi(config_st.max_works) + 1) * sizeof(struct childs));
	if (!child_st) {
		log_error("malloc childs [%d] failed:[%d]%s", (atoi(config_st.max_works) + 1), errno, strerror(errno));
		exit(1);
	}
	
	int i = 0;
	for (i=0; i<(atoi(config_st.max_works) + 1); i++) {
		child_st[i].used = 0;
		child_st[i].pid = -1;
		child_st[i].client_fd = -1;
		child_st[i].pipe_r_in = -1;
		child_st[i].pipe_r_out = -1;
		child_st[i].pipe_w_in = -1;
		child_st[i].pipe_w_out = -1;
		memset(child_st[i].mid, 0, sizeof(child_st[i].mid));
		child_st[i].mfp_out = NULL;
	}

	// 开始服务
	int connfd, epfd, sockfd, n, nread;
	struct sockaddr_in local, remote;
	socklen_t addrlen;

	// 初始化buffer
	char buf[BUF_SIZE];

	size_t pbuf_len = 0;
	size_t pbuf_size = BUF_SIZE + 1;
	char *pbuf = (char *)calloc(1, pbuf_size);
	if (pbuf == NULL) {
		log_error("calloc fail: size[%d]", pbuf_size);
		exit(1);
	}

	// 创建listen socket
	int bind_port = atoi(config_st.bind_port);
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		log_error("socket failed:[%d]:%s", errno, strerror(errno));
		exit(1);
	}
	if (setnonblocking(listenfd) > 0) {
		exit(1);
	}

	bzero(&local, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_port = htons(bind_port);
	if (bind(listenfd, (struct sockaddr *)&local, sizeof(local)) < 0) {
		log_error("bind local %d failed:[%d]%s", bind_port, errno, strerror(errno));
		exit(1);
	}
	log_info("bind local %d succ", bind_port);

	if (listen(listenfd, atoi(config_st.max_works)) != 0) {
		log_error("listen fd[%d] max_number[%d] failed:[%d]%s", listenfd, atoi(config_st.max_works), errno, strerror(errno));
		exit(1);
	}
	
	// 忽略pipe信号 
	sig_pipeignore();
	// 捕抓子进程退出信号
	sig_catch(SIGCHLD, sigchld);

	// epoll create fd
	epoll_event_num = atoi(config_st.max_works) + 1;
	epoll_evts = NULL;
	epoll_fd = -1;
	epoll_nfds = -1;

	int epoll_i = 0;

	epoll_evts = (struct epoll_event *)malloc(epoll_event_num * sizeof(struct epoll_event));
	if (epoll_evts == NULL) {
		log_error("alloc epoll event failed");
    	_exit(111);
    }

	epoll_fd = epoll_create(epoll_event_num);
	if (epoll_fd == -1) {
		log_error("epoll_create max_number[%d] failed:[%d]%s", epoll_event_num, errno, strerror(errno));
		exit(1);
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = listenfd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1) {
		log_error("epoll_ctl: listen_socket failed:[%d]%s", errno, strerror(errno));
		exit(1);
	}
	epoll_num_running = 1;

	for (;;) {

		epoll_nfds = epoll_wait(epoll_fd, epoll_evts, epoll_event_num, -1);
		
		if (epoll_nfds == -1) {
			if (errno == EINTR)	{
				// 收到中断信号
				log_info("epoll_wait recive EINTR signal, continue");
				continue;
			}

			_exit(111);
		}

		log_debug("epoll_num_running:%d nfds:%d", epoll_num_running, epoll_nfds);
		for (epoll_i = 0; epoll_i < epoll_nfds; epoll_i++) {
			sig_childblock();

			int evt_fd = epoll_evts[epoll_i].data.fd;
			if (evt_fd == listenfd) {
				if ((connfd = accept(listenfd, (struct sockaddr *) &remote, &addrlen)) > 0) {
					char *ipaddr = inet_ntoa(remote.sin_addr);
					log_info("accept client:%s", ipaddr);
					
					char greetting[512] = {0};
					char hostname[1024] = {0};
					if (gethostname(hostname, sizeof(hostname)) != 0) {
						snprintf(hostname, sizeof(hostname), "unknown");
					}

					// 取客户端IP,Port
					char client_ip[20] = {0};
					char client_port[20] = {0};
					struct sockaddr_in sa;
					int len = sizeof(sa);
					if (!getpeername(connfd, (struct sockaddr *)&sa, &len)) {
						snprintf(client_ip, sizeof(client_ip), "%s", inet_ntoa(sa.sin_addr));
						snprintf(client_port, sizeof(client_port), "%d", ntohs(sa.sin_port));
					}

					
					// 取一个新的client
					int i = new_idx_from_child_st();
					if (i == -1) {
						log_error("new_idx_from_client_st fail: maybe client queue is full.");

						// send error
						snprintf(greetting, sizeof(greetting), "%s ERR %s%s", TAG_GREET, hostname, DATA_END);
						write(connfd, greetting, strlen(greetting));
						log_debug("send fd[%d]:%s", connfd, greetting);

						continue;		
					}
					child_st[i].used = 1;
					
					int pir[2];
					int piw[2];
					if (pipe(pir) == -1) {
						log_error("unable to create pipe:%s", strerror(errno));
					
						// send error
						snprintf(greetting, sizeof(greetting), "%s ERR %s%s", TAG_GREET, hostname, DATA_END);
						write(connfd, greetting, strlen(greetting));
						log_debug("send fd[%d]:%s", connfd, greetting);
						
						continue;
					}
					if (pipe(piw) == -1) {
						log_error("unable to create pipe:%s", strerror(errno));

						close(pir[0]);
						close(pir[1]);
						pir[0] = -1;
						pir[1] = -1;

						// send error
                        snprintf(greetting, sizeof(greetting), "%s ERR %s%s", TAG_GREET, hostname, DATA_END);
                        write(connfd, greetting, strlen(greetting));
                        log_debug("send fd[%d]:%s", connfd, greetting);

                        continue;
					}
					log_debug("create pir[0]:%d pir[1]:%d", pir[0], pir[1]);
					log_debug("create piw[0]:%d piw[1]:%d", piw[0], piw[1]);
					
					
					// 当程序执行exec函数时本fd将被系统自动关闭,表示不传递给exec创建的新进程
					//fcntl(pir[0], F_SETFD, FD_CLOEXEC);			
					//fcntl(piw[1], F_SETFD, FD_CLOEXEC);			

					int f = fork();
					if (f < 0) {
						log_error("fork fail:%s", strerror(errno));

						close(pir[0]);
						close(pir[1]);
						pir[0] = -1;
						pir[1] = -1;

						close(piw[0]);
						close(piw[1]);
						piw[0] = -1;
						piw[1] = -1;
						
						child_st[i].used = 0;

						// send error
                        snprintf(greetting, sizeof(greetting), "%s ERR %s%s", TAG_GREET, hostname, DATA_END);
                        write(connfd, greetting, strlen(greetting));
                        log_debug("send fd[%d]:%s", connfd, greetting);

                        continue;
					}

					struct timeval tm;
					gettimeofday(&tm, NULL);
					snprintf(child_st[i].mid, sizeof(child_st[i].mid), "%05u%u", (uint32_t)tm.tv_usec, (uint32_t)f);
					snprintf(msg_id, sizeof(msg_id), "%s", child_st[i].mid);

					if (f == 0) {
						// 子进程
						close(pir[0]);
						close(piw[1]);
						close(listenfd);

						char _cmid[512] = {0};
						char _cchild_st[512] = {0};
						char _ccip[20] = {0};
						char _ccport[20] = {0};

						snprintf(_cmid, sizeof(_cmid), "-m%s", child_st[i].mid);
						snprintf(_cchild_st, sizeof(_cchild_st), "-c%s", config_st.child_cf);
						snprintf(_ccip, sizeof(_ccip), "-i%s", client_ip);
						snprintf(_ccport, sizeof(_ccport), "-p%s", client_port);

						char *args[6];
						args[0] = CHILD_PROG;
						args[1] = _cmid;
						args[2] = _cchild_st;
						args[3] = _ccip;
						args[4] = _ccport;
						args[5] = 0;

						if (fd_move(0, connfd) == -1) {
							log_info("%s fd_move(0, %d) failed:%d %s", child_st[i].mid, connfd, errno, strerror(errno));
							_exit(111);
						}

						if (fd_move(1, pir[1]) == -1) {
							log_info("%s fd_move(pipe_w, %d) failed:%d %s", child_st[i].mid, pir[1], errno, strerror(errno));
							_exit(111);
						}
						if (fd_move(2, piw[0]) == -1) {
							log_info("%s fd_move(pipe_r, %d) failed:%d %s", child_st[i].mid, piw[0], errno, strerror(errno));
							_exit(111);
						}

						if (execvp(*args, args) == -1) {
							log_error("execp fail:%s", strerror(errno));
							_exit(111);
						}
						//if (error_temp(errno))
						//	_exit(111);
						log_debug("execp succ");
						_exit(100);							

					}

					close(connfd);
			
					child_st[i].pipe_r_in = pir[0];
					close(pir[1]);
					pir[1] = -1;
					child_st[i].pipe_r_out = -1;

					child_st[i].pipe_w_out = piw[1];
					close(piw[0]);
					piw[0] = -1;
					child_st[i].pipe_w_in = -1;


					child_st[i].pid = f;

					
					if (setnonblocking(child_st[i].pipe_r_in) != 0) {
						log_error("setnonblocking fd[%d] failed", child_st[i].pipe_r_in);
					}

					
					struct epoll_event pipe_in_ev;
					pipe_in_ev.events = EPOLLIN | EPOLLET;	
					pipe_in_ev.data.fd = child_st[i].pipe_r_in;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipe_in_ev.data.fd, &pipe_in_ev) == -1) {
						log_error("epoll_ctl client fd[%d] EPOLL_CTL_ADD failed:[%d]%s", pipe_in_ev.data.fd, errno, strerror(errno));
					}
					log_debug("epoll_add fd[%d]", pipe_in_ev.data.fd);

					

				} else if (connfd == -1) {
					if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR) {
						log_error("accept failed:[%d]%s", errno, strerror(errno));
					}

					continue;
				}

			} else if (epoll_evts[epoll_i].events & EPOLLIN) {
				
				int ci = get_idx_with_sockfd(evt_fd);
				if (ci < 0) {
					log_error("socket fd[%d] get_idx_with_sockfd fail", evt_fd);
					continue;
				}	
				log_debug("%s get event EPOLLIN: epoll_i[%d] fd[%d] get d_i[%d] fd[%d], used[%d]", child_st[ci].mid, epoll_i, epoll_evts[epoll_i].data.fd, child_st[ci].pipe_r_in, child_st[ci].used);

				// 读取内容 ----------------------
				char inbuf[BUF_SIZE] = {0};
				char *pinbuf = inbuf;
				int inbuf_size = BUF_SIZE;
				int inbuf_len = 0;
				char ch;
				
				
				do {
					i = 0;
					nread = read(child_st[ci].pipe_r_in, inbuf, inbuf_size);
					log_debug("%s read from liao_command:[%d]", child_st[ci].mid, nread);

					if (nread == -1) {
						// read error on a readable pipe? be serious
						//log_error("it is failed from fd[%d] read. error:%d %s", child_st[ci].fd_in, errno, strerror(errno));
						//log_debug("it is failed from fd[%d] read. error:%d %s", child_st[ci].pipe_r_in, errno, strerror(errno));
						break;

					} else if (nread > 0) {
						if (child_st[ci].mfp_out == NULL) {
							child_st[ci].mfp_out = mopen(MAX_BLOCK_SIZE, NULL, NULL);
							if (child_st[ci].mfp_out == NULL) {
								log_debug("%s mopen for body fail", child_st[ci].mid);
								break;
							}
						}	

						while (i < nread) {	
							ch = inbuf[i];	
							
							if ((inbuf_size - inbuf_len) < 2) {
								int r = fast_write(child_st[ci].mfp_out, inbuf, inbuf_len);
								if (r == 0) {
									log_error("%s fast_write fail", child_st[ci].mid);
									break;
								}	

								memset(inbuf, 0, inbuf_size);
								inbuf_len = 0;
							}	

							mybyte_copy(pinbuf + inbuf_len, 1, &ch);
							inbuf_len++;	

							if (ch == '\n') {
								if (inbuf_len > 0) {
									int r = fast_write(child_st[ci].mfp_out, inbuf, inbuf_len);
									if (r == 0) {
										log_error("%s fast_write fail", child_st[ci].mid);
										break;
									}

									memset(inbuf, 0, inbuf_size);
									inbuf_len = 0;
								}
								
								// 处理当前指令
								// ...
								process_system(&child_st[ci]);
								// ...
								
								// 处理完成后
								if (child_st[ci].mfp_out != NULL) {
									mclose(child_st[ci].mfp_out);
									child_st[ci].mfp_out = NULL;
									
									child_st[ci].mfp_out = mopen(MAX_BLOCK_SIZE, NULL, NULL);
									if (child_st[ci].mfp_out == NULL) {
										log_error("mopen fail for mfp_out");
										break;
									}
								}
								
								pinbuf = inbuf + 0;
							}

							i++;
						}
						
						
					} else {
						break;
					}

				} while (1);	


			} else if ((epoll_evts[epoll_i].events & EPOLLHUP) && (epoll_evts[epoll_i].data.fd != listenfd)) {

				int ci = get_idx_with_sockfd(evt_fd);
				if ( ci < 0 ) {
					log_error("get_idx_with_sockfd(%d) fail, so not write", evt_fd);

					continue;
				}
				log_debug("%s get event EPOLLHUP: epoll_i[%d] fd[%d] get d_i[%d] fd[%d], used[%d]", child_st[ci].mid, epoll_i, epoll_evts[epoll_i].data.fd, child_st[ci].pipe_r_in, child_st[ci].used);

				epoll_delete_evt(epoll_fd, child_st[ci].pipe_r_in);

				continue;

			}


		}
		sig_childunblock();

	}

	close(epoll_fd);
	close(listenfd);

	log_info("i'm finish");
    
	return 0;
}



