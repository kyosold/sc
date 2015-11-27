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
#include <sys/time.h>
#include <openssl/sha.h>
#include "mysql.h"
#include "liao_log.h"
#include "liao_utils.h"
//#include "liao_server.h"
#include "base64.h"
#include "liao_command.h"

#include "confparser.h"
#include "dictionary.h"


// -----
char myuid[128] = {0};
char self_ios_token[MAX_LINE];

dictionary *dict_conf = NULL;

struct config_t config_st; 

time_t g_last_alive_time = 0;

int epoll_fdc = -1;
int epoll_nfds = -1;
int epoll_event_num = 0;
struct epoll_event *epoll_evts = NULL;

struct session session_st;


// -----
void liao_clean();
void child_quit();
void quit(char *prepend, char *append);


// -----
void sig_catch(int sig, void (*f) () ) 
{
    struct sigaction sa;  
    sa.sa_handler = f; 
    sa.sa_flags = 0; 
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, (struct sigaction *) 0);  
}

void is_client_timeout(int sig)
{
	log_debug("check client is timeout");

	time_t cur_time;
	time(&cur_time);

	log_debug("check client is timeout: cur_time:%lld last_time:%lld", cur_time, g_last_alive_time);
	int timeout = cur_time - g_last_alive_time;
	if (timeout > config_st.client_timeout) {
		log_debug("client socket:%d timeout:%d, bye bye", session_st.client_fd, timeout);

		child_quit();
		return;
	}
	log_debug("client socket:%d timeout:%d", session_st.client_fd, timeout);
}

void set_check_client_timeout()
{
	log_debug("set check client timeout timer");

	time(&g_last_alive_time);

	// 设置超时检查定时器
    struct itimerval tout_value;
    tout_value.it_value.tv_sec = 60;    // 定时60秒
    tout_value.it_value.tv_usec = 0; 
    tout_value.it_interval.tv_sec = 60; // 之后每60秒执行一次
    tout_value.it_interval.tv_usec = 0; 

    sig_catch(SIGALRM, is_client_timeout);  
    setitimer(ITIMER_REAL, &tout_value, NULL); 
}


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
//  0:succ  1:fail
int conf_init(char *cfg_file)
{
	memset(config_st.log_level, 0, sizeof(config_st.log_level));
	config_st.client_timeout = 0;

    memset(config_st.use_mc, 0, sizeof(config_st.use_mc));
    memset(config_st.mc_server, 0, sizeof(config_st.mc_server));
    config_st.mc_port = 0;
    memset(config_st.mc_timeout, 0, sizeof(config_st.mc_timeout));

    memset(config_st.mysql_host, 0, sizeof(config_st.mysql_host));
    config_st.mysql_port = 0;
    memset(config_st.mysql_user, 0, sizeof(config_st.mysql_user));
    memset(config_st.mysql_passwd, 0, sizeof(config_st.mysql_passwd));
    memset(config_st.mysql_db, 0, sizeof(config_st.mysql_db));
    memset(config_st.mysql_timeout, 0, sizeof(config_st.mysql_timeout));

    memset(config_st.queue_path, 0, sizeof(config_st.queue_path));
    memset(config_st.queue_host, 0, sizeof(config_st.queue_host));


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

	char *pclient_timeout = dictionary_get(dict_conf, "global:client_timeout", NULL);
	if (pclient_timeout != NULL) {
		config_st.client_timeout = atoi(pclient_timeout);
	} else {
		log_warning("parse config client_timeout error, use defaut:%s", DEF_CLIENTTIMEOUT);
        config_st.client_timeout = atoi(DEF_CLIENTTIMEOUT);
	}

	// ---- queue ----
	char *pqueue_path = dictionary_get(dict_conf, "global:queue_path", NULL);
    if (pqueue_path != NULL) {
        snprintf(config_st.queue_path, sizeof(config_st.queue_path), "%s", pqueue_path);
    } else {
        log_warning("parse config queue_path error, use default:%s", DEF_QUEUEPATH);
        snprintf(config_st.queue_path, sizeof(config_st.queue_path), "%s", DEF_QUEUEPATH);
    }

    char *pqueue_host = dictionary_get(dict_conf, "global:queue_host", NULL);
    if (pqueue_host != NULL) {
        snprintf(config_st.queue_host, sizeof(config_st.queue_host), "%s", pqueue_host);
    } else {
        log_warning("parse config queue_host error, use default:%s", DEF_QUEUEHOST);
        snprintf(config_st.queue_host, sizeof(config_st.queue_host), "%s", DEF_QUEUEHOST);
    }

	// ---- mc ----
    char *puse_mc = dictionary_get(dict_conf, "global:use_mc", NULL);
    if (puse_mc != NULL) {
        snprintf(config_st.use_mc, sizeof(config_st.use_mc), "1");
    } else {
        log_warning("parse config use_mc error, so not use mc");
        snprintf(config_st.use_mc, sizeof(config_st.use_mc), "0");
    }
   
    char *pmc_server = dictionary_get(dict_conf, "global:mc_server", NULL);
    if (pmc_server != NULL) {
        snprintf(config_st.mc_server, sizeof(config_st.mc_server), "%s", pmc_server);
    } else {
        log_warning("parse config mc_server error, use defaut:%s", DEF_MCSERVER);
        snprintf(config_st.mc_server, sizeof(config_st.mc_server), "%s", DEF_MCSERVER);
    }

	char *pmc_port = (char *)memchr(config_st.mc_server, ':', strlen(config_st.mc_server));
    if (pmc_port != NULL) {
        *pmc_port = '\0';
        config_st.mc_port = atoi(pmc_port + 1);
    } else {
        log_warning("parse config mc_port error, use defaut:%s", DEF_MCPORT);
        config_st.mc_port = atoi(DEF_MCPORT);
    }

    char *pmc_timeout = dictionary_get(dict_conf, "global:mc_timeout", NULL);
    if (pmc_timeout != NULL) {
        snprintf(config_st.mc_timeout, sizeof(config_st.mc_timeout), "%s", pmc_timeout);
    } else {
        log_warning("parse config mc_timeout error, use defaut:%s", DEF_MCTIMEOUT);
        snprintf(config_st.mc_timeout, sizeof(config_st.mc_timeout), "%s", DEF_MCTIMEOUT);
    }

	// ---- mysql ----
	char *pmysql_host = dictionary_get(dict_conf, "global:mysql_host", NULL);
    if (pmysql_host != NULL) {
        snprintf(config_st.mysql_host, sizeof(config_st.mysql_host), "%s", pmysql_host);
    } else {
        log_warning("parse config mc_server error, use defaut:%s", DEF_MYSQLSERVER);
        snprintf(config_st.mysql_host, sizeof(config_st.mysql_host), "%s", DEF_MYSQLSERVER);
    }  

    char *pmysql_port = dictionary_get(dict_conf, "global:mysql_port", NULL);
    if (pmysql_port != NULL) {
        config_st.mysql_port = atoi(pmysql_port);
    } else {
        log_warning("parse config mysql_port error, use default:%s", DEF_MYSQLPORT);
        config_st.mysql_port = atoi(DEF_MYSQLPORT);
    }

    char *pmysql_user = dictionary_get(dict_conf, "global:mysql_user", NULL);
    if (pmysql_user != NULL) {
        snprintf(config_st.mysql_user, sizeof(config_st.mysql_user), "%s", pmysql_user);
    } else {
        log_warning("parse config mysql_user error, use defaut:%s", DEF_MYSQLUSER);
        snprintf(config_st.mysql_user, sizeof(config_st.mysql_user), "%s", DEF_MYSQLUSER);
    }

    char *pmysql_passwd = dictionary_get(dict_conf, "global:mysql_passwd", NULL);
    if (pmysql_passwd != NULL) {
        snprintf(config_st.mysql_passwd, sizeof(config_st.mysql_passwd), "%s", pmysql_passwd);
    } else {
        log_warning("parse config mysql_passwd error, use defaut:%s", DEF_MYSQLPASSWD);
        snprintf(config_st.mysql_passwd, sizeof(config_st.mysql_passwd), "%s", DEF_MYSQLPASSWD);
    }

    char *pmysql_db= dictionary_get(dict_conf, "global:mysql_db", NULL);
    if (pmysql_db != NULL) {
        snprintf(config_st.mysql_db, sizeof(config_st.mysql_db), "%s", pmysql_db);
    } else {
        log_warning("parse config mysql_db error, use defaut:%s", DEF_MYSQLDB);
        snprintf(config_st.mysql_db, sizeof(config_st.mysql_db), "%s", DEF_MYSQLDB);
    }

    char *pmysql_timeout = dictionary_get(dict_conf, "global:mysql_timeout", NULL);
    if (pmysql_timeout != NULL) {
        snprintf(config_st.mysql_timeout, sizeof(config_st.mysql_timeout), "%s", pmysql_timeout);
    } else {
        log_warning("parse config mysql_timeout error, use defaut:%s", DEF_MYSQLTIMEOUT);
        snprintf(config_st.mysql_timeout, sizeof(config_st.mysql_timeout), "%s", DEF_MYSQLTIMEOUT);
    }

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




// Client Command
//
//


// HELO: 用于表示客户端信息	helo uid:0000001 \r\n\r\n
int helo_cmd(struct session *session, char *pbuf, int pbuf_size, char *myuid, size_t myuid_size)
{
	log_debug("pbuf:%d:%s", pbuf_size, pbuf);

	int sockfd = session->client_fd;

	int r = 0;
	int ret;
	int n = pbuf_size;
	char uid[MAX_LINE] = {0};	

	while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {

		char *psed = (char *)memchr(pbuf, ' ', strlen(pbuf));
		if (psed != NULL) {
			*psed = '\0';
			int m = (psed - pbuf);

			// 分段取变量
			log_debug("ptok:[%s]", pbuf);

			if (strncasecmp(pbuf, "uid:", 4) == 0) {
				r = snprintf(uid, sizeof(uid), "%s", pbuf+4);
				log_debug("from uid:%d:%s", r, uid);	

			}

			pbuf = psed + 1;
			n -= (m + 1);

		} else {
			if (pbuf != NULL) {
				if (strncasecmp(pbuf, "uid:", 4) == 0) {
					r = snprintf(uid, sizeof(uid), "%s", pbuf+4);
					while ( uid[r-1] == '\r' || uid[r-1] == '\n' )
						r--;
					uid[r] = '\0';
					log_debug("from uid:%d:%s", r, uid);	
				}
			}

			break;
		}

	}

	log_debug("get sockfd[%d] from uid[%s]", sockfd, uid);

	if ( strlen(uid) <= 0 ) {		
		log_error("get sockfd[%d] parameter error", sockfd);		
		return 1;	
	}

	snprintf(myuid, myuid_size, "%s", uid);
	

	// 注册为在线	(uid => cpid)
	// cpid: current pid 该进程pid号，用于进程间传递消息通知
    snprintf(session->uid, sizeof(session->uid), "%s", uid);

	pid_t cpid = getpid();
    char mc_value[MAX_LINE] = {0}; 
    snprintf(mc_value, sizeof(mc_value), "%d", cpid);

	ret = set_to_mc(config_st.mc_server, config_st.mc_port, atoi(config_st.mc_timeout), uid, mc_value);
	if (ret != 0) {
		log_error("add to online failed uid[%s]=>cpid[%ld] sockfd[%d]", uid, cpid, sockfd);	
		return 1;
	} else {
		log_debug("registon online succ uid[%s]=>cpid[%ld] sockfd[%d]", uid, sockfd);	

		return 0;
	}
	
}


// quit uid:0000001 
int quit_cmd(struct session *session, char *pbuf, int pbuf_size)
{
	log_debug("pbuf:%d:%s", pbuf_size, pbuf);

	int sockfd = session->client_fd;

	int r = 0;
	int n = pbuf_size;
	char uid[MAX_LINE] = {0};

	while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {

		char *psed = (char *)memchr(pbuf, ' ', strlen(pbuf));
		if (psed != NULL) {
			*psed = '\0';
			int m = (psed - pbuf);

			// 分段取变量
			log_debug("ptok:[%s]", pbuf);

			if (strncasecmp(pbuf, "uid:", 4) == 0) {
				r = snprintf(uid, sizeof(uid), "%s", pbuf+4);
				log_debug("from uid:%d:%s", r, uid);	
			}

			pbuf = psed + 1;
			n -= (m + 1);

		} else {
			if (pbuf != NULL) {
				if (strncasecmp(pbuf, "uid:", 4) == 0) {
					r = snprintf(uid, sizeof(uid), "%s", pbuf+4);
					while ( uid[r-1] == '\r' || uid[r-1] == '\n' )
                        r--;
                    uid[r] = '\0';
					log_debug("from uid:%d:%s", r, uid);

				}	
			}
			break;
		}

	}
	
	// 设置离线
	// 查询fd
	if (strcasecmp(session->uid, uid) != 0) {
		log_error("logic error session.uid:%s != uid:%s", session->uid, uid);
		return 1;
	}

	// 注册为离线	(uid => ios_token)
	int ret = delete_from_mc(config_st.mc_server, config_st.mc_port, atoi(config_st.mc_timeout), uid);
	if (ret != 0) {
		log_error("delete from online failed uid[%s] sockfd[%d]", uid, sockfd);	
		return 1;

	}
	log_debug("offline fuid:%s", uid);

	//close(sockfd);

	//_exit(100);
	
	return 0;
		
}


int send_notice_to_other_progress(struct session *session, char *pbuf, int pbuf_size)
{
	log_debug("pbuf:%d:%s", pbuf_size, pbuf);
	
	int r = 0;
	int n = pbuf_size;
	
	char pid[MAX_LINE] = {0};
	char mqid[MAX_LINE] = {0};
	char fuser_type[MAX_LINE] = {0};	
	char fuid[MAX_LINE] = {0};	
	char fnick[MAX_LINE] = {0}; 
	char tuid[MAX_LINE] = {0}; 
	char m_type[MAX_LINE] = {0};
	
	while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {

		char *psed = (char *)memchr(pbuf, ' ', strlen(pbuf));
		if (psed != NULL) {
			*psed = '\0';
			int m = (psed - pbuf);

			// 分段取变量
			log_debug("ptokx:[%s]", pbuf);

			if (strncasecmp(pbuf, "pid:", 4) == 0) {
				r = snprintf(pid, sizeof(pid), "%s", pbuf+4);
				log_debug("from pid:%d:%s", r, pid);

			} else if (strncasecmp(pbuf, "mqid:", 5) == 0) {
				r = snprintf(mqid, sizeof(mqid), "%s", pbuf+5);
				log_debug("from mqid:%d:%s", r, mqid);

			} else if (strncasecmp(pbuf, "fuser_type:", 11) == 0) {
				r = snprintf(fuser_type, sizeof(fuser_type), "%s", pbuf+11);
				log_debug("from fuser_type:%d:%s", r, fuser_type);

			} else if (strncasecmp(pbuf, "fuid:", 5) == 0) {
				r = snprintf(fuid, sizeof(fuid), "%s", pbuf+5);
				log_debug("from uid:%d:%s", r, fuid);	

			} else if (strncasecmp(pbuf, "fnick:", 6) == 0) {
				r = snprintf(fnick, sizeof(fnick), "%s", pbuf+6);
				log_debug("from fnick:%d:%s", r, fnick);

			} else if (strncasecmp(pbuf, "mtype:", 6) == 0) {
				r = snprintf(m_type, sizeof(m_type), pbuf+6);
				log_debug("from mtype:%d:%s", r, m_type);	
				
			} else if (strncasecmp(pbuf, "tuid:", 5) == 0) {
				r = snprintf(tuid, sizeof(tuid), "%s", pbuf+5);
				log_debug("from tuid:%d:%s", r, tuid);

			}

			pbuf = psed + 1;
			n -= (m + 1);

		} else {
			if (pbuf != NULL) {
				if (strncasecmp(pbuf, "tuid:", 5) == 0) {
					r = snprintf(tuid, sizeof(tuid), "%s", pbuf+5);
					while ( tuid[r-1] == '\r' || tuid[r-1] == '\n' )
                        r--;
                    tuid[r] = '\0';
					log_debug("from tuid:%d:%s", r, tuid);
				}
			}

			break;
		}

	}
	
	// 
	//n = send_msg_to_another_process(session->parent_out_fd, pid, fuid, fnick, m_type, tuid);
	n = send_msg_to_another_process_v2(session->parent_out_fd, pid, mqid, fuser_type, fuid, fnick, m_type, tuid);
	if (n == 0) {
		return 0;
	}
	
	return 1;
}


// SENDADDFRDREQ mid:%@ fuid:%@ fnick:%@ tuid:%@ \r\r\r\n
int sendreq_addfriend(struct session *session, char *pbuf, int pbuf_size, char *mid, size_t mid_size, int *qid)
{
	log_debug("pbuf:%d:%s", pbuf_size, pbuf);

	char m_type[] = "SYS";
	int r = 0;
	int n = pbuf_size;
	
	char fuid[MAX_LINE] = {0};	
	char fnick[MAX_LINE] = {0}; 
	char tuid[MAX_LINE] = {0}; 

	while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {

		char *psed = (char *)memchr(pbuf, ' ', strlen(pbuf));
		if (psed != NULL) {
			*psed = '\0';
			int m = (psed - pbuf);

			// 分段取变量
			log_debug("ptok:[%s]", pbuf);

			if (strncasecmp(pbuf, "mid:", 4) == 0) {
				r = snprintf(mid, mid_size, "%s", pbuf+4);
				log_debug("from mid:%d:%s", r, mid);

			} else if (strncasecmp(pbuf, "fuid:", 5) == 0) {
				r = snprintf(fuid, sizeof(fuid), "%s", pbuf+5);
				log_debug("from uid:%d:%s", r, fuid);	

			} else if (strncasecmp(pbuf, "fnick:", 6) == 0) {
				r = snprintf(fnick, sizeof(fnick), "%s", pbuf+6);
				log_debug("from fnick:%d:%s", r, fnick);

			} else if (strncasecmp(pbuf, "tuid:", 5) == 0) {
				r = snprintf(tuid, sizeof(tuid), "%s", pbuf+5);
				log_debug("from tuid:%d:%s", r, tuid);

			}

			pbuf = psed + 1;
			n -= (m + 1);

		} else {
			if (pbuf != NULL) {
				if (strncasecmp(pbuf, "tuid:", 5) == 0) {
					r = snprintf(tuid, sizeof(tuid), "%s", pbuf+5);
					while ( tuid[r-1] == '\r' || tuid[r-1] == '\n' )
                        r--;
                    tuid[r] = '\0';
					log_debug("from tuid:%d:%s", r, tuid);
				}
			}

			break;
		}

	}


	// 写消息索引到数据库
	n = write_queue_to_db_v2(&config_st, mid, m_type, fuid, fnick, tuid, "rmf", "", 0, "");
	if (n == -1) {
		goto SNDFRD_FAIL;
	}
	*qid = n;

	// 获取头像地址
	log_debug("send notice message to tuid:%s", tuid);
	char ficon[MAX_LINE] = {0};
	n = get_avatar_url(&config_st, fuid, ficon, sizeof(ficon));
	if (n == 0) {
		log_error("get_avatar_url get fail");
	}

	// 检查收件人是否在线
	char mc_value[MAX_LINE] = {0};
	int ret = get_to_mc(config_st.mc_server, config_st.mc_port, atoi(config_st.mc_timeout), tuid, mc_value, sizeof(mc_value));	
	if (ret == 0) {

		if ( *mc_value != '\0' ) {
			char *tpid = mc_value;

			// 收件人在线，使用socket发送通知
			log_debug("tuid:%s is online, use socket send message", tuid);
			n = send_msg_to_another_process(session->parent_out_fd, tpid, fuid, fnick, m_type, tuid);
			if (n == 0) {
				goto SNDFRD_SUCC;
			}

			log_error("use send_socket_cmd send notice fail, use APN send");
			
		}

	}

	// 收件人不在线，使用APN通知
	log_debug("tuid:%s not online, use APNS send msg", tuid);

	send_apn_cmd(&config_st, fuid, fnick, m_type, tuid);
	

	goto SNDFRD_SUCC;

	//n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "SSYSTEM SENDMSG tuid:%s msg:%s", tuid );
	//n = send_to_socket(session_st->parent_out_fd, rspsoneBuf, n);


SNDFRD_FAIL:
	return 1;

SNDFRD_SUCC:
	return 0;

}


int sendget_newmsg(struct session *session, char *pbuf, int pbuf_size, char *mid, int mid_size)
{
	log_debug("pbuf:%d:%s", pbuf_size, pbuf);

	char m_type[] = "SYS";
	int r = 0;
	int n = pbuf_size;
	char fuid[MAX_LINE] = {0};	
	//char fios_token[MAX_LINE] = {0};	
	char fnick[MAX_LINE] = {0}; 
	char tuid[MAX_LINE] = {0}; 
	//char tios_token[MAX_LINE] = {0}; 

	while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {

		char *psed = (char *)memchr(pbuf, ' ', strlen(pbuf));
		if (psed != NULL) {
			*psed = '\0';
			int m = (psed - pbuf);

			// 分段取变量
			log_debug("ptok:[%s]", pbuf);

			if (strncasecmp(pbuf, "mid:", 4) == 0) {
				r = snprintf(mid, mid_size, "%s", pbuf+4);
				log_debug("from mid:%d:%s", r, mid);

			} else if (strncasecmp(pbuf, "fuid:", 5) == 0) {
				r = snprintf(fuid, sizeof(fuid), "%s", pbuf+5);
				log_debug("from uid:%d:%s", r, fuid);	

			//} else if (strncasecmp(pbuf, "fios_token:", 11) == 0) {
			//	r = snprintf(fios_token, sizeof(fios_token), "%s", pbuf+11);
			//	log_debug("from ios_token:%d:%s", r, fios_token);

			} else if (strncasecmp(pbuf, "fnick:", 6) == 0) {
				r = snprintf(fnick, sizeof(fnick), "%s", pbuf+6);
				log_debug("from fnick:%d:%s", r, fnick);

			} else if (strncasecmp(pbuf, "tuid:", 5) == 0) {
				r = snprintf(tuid, sizeof(tuid), "%s", pbuf+5);
				log_debug("from tuid:%d:%s", r, tuid);

			//} else if (strncasecmp(pbuf, "tios_token:", 11) == 0) {
			//	r = snprintf(tios_token, sizeof(tios_token), "%s", pbuf+11);
			//	log_debug("from tios_token:%d:%s", r, tios_token);

			}

			pbuf = psed + 1;
			n -= (m + 1);

		} else {
			if (pbuf != NULL) {
				if (strncasecmp(pbuf, "tuid:", 5) == 0) {
					r = snprintf(tuid, sizeof(tuid), "%s", pbuf+5);
					while ( tuid[r-1] == '\r' || tuid[r-1] == '\n' )
                        r--;
                    tuid[r] = '\0';
					log_debug("from tios_token:%d:%s", r, tuid);
				}
			}

			break;
		}

	}


	// 检查收件人是否在线
	char mc_value[MAX_LINE] = {0};
	int ret = get_to_mc(config_st.mc_server, config_st.mc_port, atoi(config_st.mc_timeout), tuid, mc_value, sizeof(mc_value));	
	if (ret == 0) {

		/*char *pios_token = NULL;
		char tpid[MAX_LINE] = {0};
		int i = 0;
		while ( (mc_value[i] != ',') && (i < strlen(mc_value)) && (i < sizeof(tpid)) ) {
			tpid[i] = mc_value[i];
			i++;
		}
		i++;

		if ( (i < strlen(mc_value)) && ((pios_token = mc_value + i) != '\0') ) {*/
		if ( *mc_value != '\0' ) {
			char *tpid = mc_value;

			// 收件人在线，使用socket发送通知
			log_debug("tuid:%s is online, use socket send message", tuid);
			n = send_msg_to_another_process(session->parent_out_fd, tpid, fuid, fnick, m_type, tuid);
			if (n == 0) {
				goto SNDMSG_SUCC;
			}

			log_error("use send_socket_cmd send notice fail, use APN send");
			
		}


	}

	// 收件人不在线，使用APN通知
	log_debug("tuid:%s not online, use APNS send msg", tuid);
	//send_apn_cmd(tios_token, fuid, fnick, m_type, tuid);
	send_apn_cmd(&config_st, fuid, fnick, m_type, tuid);
	

	goto SNDMSG_SUCC;



SNDMSG_FAIL:
	return 1;

SNDMSG_SUCC:
	return 0;


}



// sendtxt mid:xxxxx fuid:0000001 fios_token:xxxxxxxxxxxxxx fnick:xxxxx tuid:0000220 tios_token:ccccccccccccccc message:32:abcabcabc
int sendtxt_cmd(struct session *session, char *pbuf, int pbuf_size, char *mid, size_t mid_size, char *qid, size_t qid_size)
{
	log_debug("pbuf:%d:%s", pbuf_size, pbuf);

	//char m_type[] = "TXT";

	int r = 0;
	int n = pbuf_size;
	char m_type[MAX_LINE] = {0};
	char fuid[MAX_LINE] = {0};	
	char fios_token[MAX_LINE] = {0};	
	char fnick[MAX_LINE] = {0}; 
	char tuid[MAX_LINE] = {0}; 
	char tios_token[MAX_LINE] = {0}; 
	char *pmessage = NULL;

	while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {

		char *psed = (char *)memchr(pbuf, ' ', strlen(pbuf));
		if (psed != NULL) {
			*psed = '\0';
			int m = (psed - pbuf);

			// 分段取变量
			log_debug("ptok:[%s]", pbuf);

			if (strncasecmp(pbuf, "mid:", 4) == 0) {
				r = snprintf(mid, mid_size, "%s", pbuf+4);
				log_debug("from mid:%d:%s", r, mid);
			
			} else if (strncasecmp(pbuf, "qtype:", 6) == 0) {
				r = snprintf(m_type, sizeof(m_type), "%s", pbuf+6);
				log_debug("from m_type:%d:%s", r, m_type);

			} else if (strncasecmp(pbuf, "fuid:", 5) == 0) {
				r = snprintf(fuid, sizeof(fuid), "%s", pbuf+5);
				log_debug("from uid:%d:%s", r, fuid);	

			} else if (strncasecmp(pbuf, "fios_token:", 11) == 0) {
				r = snprintf(fios_token, sizeof(fios_token), "%s", pbuf+11);
				log_debug("from ios_token:%d:%s", r, fios_token);

			} else if (strncasecmp(pbuf, "fnick:", 6) == 0) {
				r = snprintf(fnick, sizeof(fnick), "%s", pbuf+6);
				log_debug("from fnick:%d:%s", r, fnick);

			} else if (strncasecmp(pbuf, "tuid:", 5) == 0) {
				r = snprintf(tuid, sizeof(tuid), "%s", pbuf+5);
				log_debug("from tuid:%d:%s", r, tuid);

			} else if (strncasecmp(pbuf, "tios_token:", 11) == 0) {
				r = snprintf(tios_token, sizeof(tios_token), "%s", pbuf+11);
				log_debug("from tios_token:%d:%s", r, tios_token);

			}

			pbuf = psed + 1;
			n -= (m + 1);

		} else {
			if (pbuf != NULL) {
				if (strncasecmp(pbuf, "message:", 8) == 0) {
					pmessage = pbuf + 8;
					n -= 8;
					log_debug("from message1:%d:%s", n, pmessage);
				}
			}

			break;
		}

	}
	

	// 写消息到队列
	char queue_file[MAX_LINE] = {0};
	n = write_content_to_file_with_uid_mfile(&config_st, tuid, pmessage, n, session->mfp_in, queue_file, sizeof(queue_file));
	if (n == -1) {
		log_error("write_content_to_file_with_uid fail");
		goto TXT_FAIL;
	}

	// 写消息索引到数据库
	unsigned long queue_size = n;
    struct stat queue_st;
    if ( lstat(queue_file, &queue_st) == 0 )
        queue_size = queue_st.st_size;

	int mqid = write_queue_to_db(&config_st, TAG_RECVTXT, mid, fuid, fnick, fios_token, tuid, tios_token, m_type, queue_file, queue_size);
	if (mqid == -1) {
		goto TXT_FAIL;
	}
	snprintf(qid, qid_size, "%d", mqid);

	// 发送通知
	// ...

	// 检查收件人是否在线
	char mc_value[MAX_LINE] = {0};
	int ret = get_to_mc(config_st.mc_server, config_st.mc_port, atoi(config_st.mc_timeout), tuid, mc_value, sizeof(mc_value));	
	if (ret == 0) {

		/*char *pios_token = NULL;
		char tpid[MAX_LINE] = {0};
		int i = 0;
		while ( (mc_value[i] != ',') && (i < strlen(mc_value)) && (i < sizeof(tpid)) ) {
			tpid[i] = mc_value[i];
			i++;
		}
		i++;

		if ( (i < strlen(mc_value)) && ((pios_token = mc_value + i) != '\0') ) {*/
		if ( *mc_value != '\0' ) {
			char *tpid = mc_value;

			// 收件人在线，使用socket发送通知
			log_debug("tuid:%s is online, use socket send message", tuid);
			n = send_msg_to_another_process(session->parent_out_fd, tpid, fuid, fnick, m_type, tuid);
			if (n == 0) {
				goto TXT_SUCC;
			}

			log_error("use send_socket_cmd send notice fail, use APN send");
			
		}


	}

	// 收件人不在线，使用APN通知
	log_debug("tuid:%s not online, use APNS send msg", tuid);
	//send_apn_cmd(tios_token, fuid, fnick, m_type, tuid);
	send_apn_cmd(&config_st, fuid, fnick, m_type, tuid);
	

	goto TXT_SUCC;



TXT_FAIL:
	if (strlen(queue_file) > 0) {
		unlink(queue_file);
	}

	return 1;

TXT_SUCC:
	return 0;

}


// sendimg mid:xxxxx fuid:0000001 fios_token:xxxxxxxxxxxxxx fnick:xxxx tuid:0000220 tios_token:ccccccccccccccc file_name:abcd.JPG message:abcabcabc
int sendimg_cmd(struct session *session, char *pbuf, size_t pbuf_size, char *mid, size_t mid_size, char *thumb_name_url, size_t thumb_name_url_size, char *qid, size_t qid_size)
{
	log_debug("pbuf:%d:%s", pbuf_size, pbuf);

	//char m_type[] = "IMG";

	int r = 0;
	int n = pbuf_size;
	char m_type[MAX_LINE] = {0};
	char fuid[MAX_LINE] = {0};	
	char fios_token[MAX_LINE] = {0};	
	char fnick[MAX_LINE] = {0}; 
	char tuid[MAX_LINE] = {0}; 
	char tios_token[MAX_LINE] = {0}; 
	char file_name[MAX_LINE] = {0};
	char *pmessage = NULL;

	while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {

		char *psed = (char *)memchr(pbuf, ' ', strlen(pbuf));
		if (psed != NULL) {
			*psed = '\0';
			int m = (psed - pbuf);

			// 分段取变量
			log_debug("ptok:[%s]", pbuf);

			if (strncasecmp(pbuf, "mid:", 4) == 0) {
				r = snprintf(mid, mid_size, "%s", pbuf+4);
				log_debug("from mid:%d:%s", r, mid);

			} else if (strncasecmp(pbuf, "qtype:", 6) == 0) {
				r = snprintf(m_type, sizeof(m_type), "%s", pbuf+6);
				log_debug("from m_type:%d:%s", r, m_type);

			} else if (strncasecmp(pbuf, "fuid:", 5) == 0) {
				r = snprintf(fuid, sizeof(fuid), "%s", pbuf+5);
				log_debug("from uid:%d:%s", r, fuid);	

			} else if (strncasecmp(pbuf, "fios_token:", 11) == 0) {
				r = snprintf(fios_token, sizeof(fios_token), "%s", pbuf+11);
				log_debug("from ios_token:%d:%s", r, fios_token);

			} else if (strncasecmp(pbuf, "fnick:", 6) == 0) {
				r = snprintf(fnick, sizeof(fnick), "%s", pbuf+6);
				log_debug("from fnick:%d:%s", r, fnick);

			} else if (strncasecmp(pbuf, "tuid:", 5) == 0) {
				r = snprintf(tuid, sizeof(tuid), "%s", pbuf+5);
				log_debug("from tuid:%d:%s", r, tuid);

			} else if (strncasecmp(pbuf, "tios_token:", 11) == 0) {
				r = snprintf(tios_token, sizeof(tios_token), "%s", pbuf+11);
				log_debug("from tios_token:%d:%s", r, tios_token);

			} else if (strncasecmp(pbuf, "file_name:", 10) == 0) {
				r = snprintf(file_name, sizeof(file_name), "%s", pbuf+10);
				log_debug("from file_name:%d:%s", r, file_name);

			}

			pbuf = psed + 1;
			n -= (m + 1);

		} else {
			if (pbuf != NULL) {
				if (strncasecmp(pbuf, "message:", 8) == 0) {
					pmessage = pbuf + 8;
					n -= 8;
					log_debug("from message1:%d:%s", n, pmessage);
				}
			}

			break;
		}

	}


	// 为图片做base64解码
	char img_name[MAX_LINE] = {0};
	char file_path[MAX_LINE] = {0};
	snprintf(file_path, sizeof(file_path), "%s/%s/images/", config_st.queue_path, tuid);

	n = write_b64_content_to_file_with_uid_mfile(file_path, pmessage, n, session->mfp_in, file_name, img_name, sizeof(img_name));
	if (n == -1) {
		log_error("base64_decode or write original image to file fail");
		goto IMG_FAIL;
	}


	// 生成缩略图
    char orig_path_name[MAX_LINE] = {0}; 
    char thumb_path_name[MAX_LINE] = {0}; 
    snprintf(orig_path_name, sizeof(orig_path_name), "%s/%s", file_path, img_name);
    snprintf(thumb_path_name, sizeof(thumb_path_name), "%s/s_%s", file_path, img_name);
	int ret = image_resize_without_scale(orig_path_name, thumb_path_name, 95);
	if (ret != 0) {
		log_error("create thumb fail:[%s] -> [%s]", orig_path_name, thumb_path_name);
		goto IMG_FAIL;
	}

	// # http://sc.vmeila.com/api/data/11000/images/s_142911925207076000.jpg
	snprintf(thumb_name_url, thumb_name_url_size, "%s/%s/images/s_%s", config_st.queue_host, tuid, img_name);


	// 写消息索引到数据库	
	unsigned long queue_size = n;
	struct stat queue_st;
	if (stat(thumb_path_name, &queue_st) == 0)
		queue_size = queue_st.st_size;

	int mqid = write_queue_to_db(&config_st, TAG_RECVIMG, mid, fuid, fnick, fios_token, tuid, tios_token, m_type, thumb_name_url, queue_size);
	if (n == -1) {
		goto IMG_FAIL;
	}
	snprintf(qid, qid_size, "%d_%s_%s", mqid, fuid, tuid);


	// 发送通知
	log_debug("send notice message to tuid:%s", tuid);

	// 检查收件人是否在线
	char mc_value[MAX_LINE] = {0};
	ret = get_to_mc(config_st.mc_server, config_st.mc_port, atoi(config_st.mc_timeout), tuid, mc_value, sizeof(mc_value));	
	if (ret == 0) {

		/*char *pios_token = NULL;
		char tpid[MAX_LINE] = {0};
		int i = 0;
		while ( (mc_value[i] != ',') && (i < strlen(mc_value)) && (i < sizeof(tpid)) ) {
			tpid[i] = mc_value[i];
			i++;
		}
		i++;

		if ( (i < strlen(mc_value)) && ((pios_token = mc_value + i) != '\0') ) {*/
		if ( *mc_value != '\0' ) {
			char *tpid = mc_value;

			// 收件人在线，使用socket发送通知
			log_debug("tuid:%s is online, use socket send message", tuid);
			n = send_msg_to_another_process(session->parent_out_fd, tpid, fuid, fnick, m_type, tuid);
			if (n == 0) {
				goto IMG_SUCC;
			}

			log_error("use send_socket_cmd send notice fail, use APN send");
			
		}


	}

	// 收件人不在线，使用APN通知
	log_debug("tuid:%s not online, use APNS send msg", tuid);
	//send_apn_cmd(tios_token, fuid, fnick, m_type, tuid);
	send_apn_cmd(&config_st, fuid, fnick, m_type, tuid);

	goto IMG_SUCC;
	


IMG_FAIL:
	return 1;


IMG_SUCC:
	return 0;

}


// sendaudio mid:xxxxx fuid:0000001 fios_token:xxxxxxxxxxxxxx fnick:xxxx tuid:0000220 tios_token:ccccccccccccccc file_name:1429860634.spx message:abcabcabc
int sendaudio_cmd(struct session *session, char *pbuf, size_t pbuf_size, char *mid, size_t mid_size, char *audio_name_url, size_t audio_name_url_size, char *qid, size_t qid_size)
{
	log_debug("pbuf:%d:%s", pbuf_size, pbuf);

	//char m_type[] = "AUDIO";

	int r = 0;
	int n = pbuf_size;
	char m_type[MAX_LINE] = {0};	
	char fuid[MAX_LINE] = {0};	
	char fios_token[MAX_LINE] = {0};	
	char fnick[MAX_LINE] = {0}; 
	char tuid[MAX_LINE] = {0}; 
	char tios_token[MAX_LINE] = {0}; 
	char file_name[MAX_LINE] = {0};
	char audio_size[MAX_LINE] = {0};
	char *pmessage = NULL;

	while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {

		char *psed = (char *)memchr(pbuf, ' ', strlen(pbuf));
		if (psed != NULL) {
			*psed = '\0';
			int m = (psed - pbuf);

			// 分段取变量
			log_debug("ptok:[%s]", pbuf);

			if (strncasecmp(pbuf, "mid:", 4) == 0) {
				r = snprintf(mid, mid_size, "%s", pbuf+4);
				log_debug("from mid:%d:%s", r, mid);

			} else if (strncasecmp(pbuf, "qtype:", 6) == 0) {
				r = snprintf(m_type, sizeof(m_type), "%s", pbuf+6);
				log_debug("from m_type:%d:%s", r, m_type);

			} else if (strncasecmp(pbuf, "fuid:", 5) == 0) {
				r = snprintf(fuid, sizeof(fuid), "%s", pbuf+5);
				log_debug("from uid:%d:%s", r, fuid);	

			} else if (strncasecmp(pbuf, "fios_token:", 11) == 0) {
				r = snprintf(fios_token, sizeof(fios_token), "%s", pbuf+11);
				log_debug("from ios_token:%d:%s", r, fios_token);

			} else if (strncasecmp(pbuf, "fnick:", 6) == 0) {
				r = snprintf(fnick, sizeof(fnick), "%s", pbuf+6);
				log_debug("from fnick:%d:%s", r, fnick);

			} else if (strncasecmp(pbuf, "tuid:", 5) == 0) {
				r = snprintf(tuid, sizeof(tuid), "%s", pbuf+5);
				log_debug("from tuid:%d:%s", r, tuid);

			} else if (strncasecmp(pbuf, "tios_token:", 11) == 0) {
				r = snprintf(tios_token, sizeof(tios_token), "%s", pbuf+11);
				log_debug("from tios_token:%d:%s", r, tios_token);

			} else if (strncasecmp(pbuf, "file_name:", 10) == 0) {
				r = snprintf(file_name, sizeof(file_name), "%s", pbuf+10);
				log_debug("from file_name:%d:%s", r, file_name);

			} else if (strncasecmp(pbuf, "audio_size:", 11) == 0) {
				r = snprintf(audio_size, sizeof(audio_size), "%s", pbuf+11);
				log_debug("from audio_size:%d:%s", r, audio_size);

			}

			pbuf = psed + 1;
			n -= (m + 1);

		} else {
			if (pbuf != NULL) {
				if (strncasecmp(pbuf, "message:", 8) == 0) {
					pmessage = pbuf + 8;
					n -= 8;
					log_debug("from message1:%d:%s", n, pmessage);
				}
			}

			break;
		}

	}


	// 为音频做base64解码
	char aud_name[MAX_LINE] = {0}; 
    char file_path[MAX_LINE] = {0}; 
    snprintf(file_path, sizeof(file_path), "%s/%s/audios/", config_st.queue_path, tuid);

    n = write_b64_content_to_file_with_uid_mfile(file_path, pmessage, n, session->mfp_in, file_name, aud_name, sizeof(aud_name));
    if (n == -1) {
        log_error("base64_decode or write original audio to file fail");
        goto IMG_FAIL;
    }   


	// # http://sc.vmeila.com/api/data/11000/images/s_142911925207076000.jpg
	snprintf(audio_name_url, audio_name_url_size, "%s/%s/audios/%s", config_st.queue_host, tuid, aud_name);


	// 写消息索引到数据库	
	unsigned long queue_size = atoi(audio_size);

	int mqid = write_queue_to_db(&config_st, TAG_RECVAUDIO, mid, fuid, fnick, fios_token, tuid, tios_token, m_type, audio_name_url, queue_size);
	if (n == -1) {
		goto IMG_FAIL;
	}
	snprintf(qid, qid_size, "%d_%s_%s", mqid, fuid, tuid);


	// 发送通知
	log_debug("send notice message to tuid:%s", tuid);

	// 检查收件人是否在线
	char mc_value[MAX_LINE] = {0};
	int ret = get_to_mc(config_st.mc_server, config_st.mc_port, atoi(config_st.mc_timeout), tuid, mc_value, sizeof(mc_value));	
	if (ret == 0) {

		/*char *pios_token = NULL;
		char tpid[MAX_LINE] = {0};
		int i = 0;
		while ( (mc_value[i] != ',') && (i < strlen(mc_value)) && (i < sizeof(tpid)) ) {
			tpid[i] = mc_value[i];
			i++;
		}
		i++;

		if ( (i < strlen(mc_value)) && ((pios_token = mc_value + i) != '\0') ) {*/
		if ( *mc_value != '\0' ) {
			char *tpid = mc_value;

			// 收件人在线，使用socket发送通知
			log_debug("tuid:%s is online, use socket send message", tuid);
			n = send_msg_to_another_process(session->parent_out_fd, tpid, fuid, fnick, m_type, tuid);
			if (n == 0) {
				goto IMG_SUCC;
			}

			log_error("use send_socket_cmd send notice fail, use APN send");
			
		}


	}

	// 收件人不在线，使用APN通知
	log_debug("tuid:%s not online, use APNS send msg", tuid);
	//send_apn_cmd(tios_token, fuid, fnick, m_type, tuid);
	send_apn_cmd(&config_st, fuid, fnick, m_type, tuid);

	goto IMG_SUCC;
	


IMG_FAIL:
	return 1;


IMG_SUCC:
	return 0;

}


// deletescmdata mid:xxxxx fuid:0000001 fios_token:xxxxxxxxxxxxxx fnick:xxxx tuid:0000220 tios_token:ccccccccccccccc qids:123,234,442
int deleteSCMData_cmd(struct session *session, char *pbuf, size_t pbuf_size)
{
	log_debug("pbuf:%d:%s", pbuf_size, pbuf);

	char m_type[] = "DELETESCM";

	int r = 0;
	int n = pbuf_size;
	char mid[MAX_LINE] = {0};
	char fuid[MAX_LINE] = {0};	
	char fios_token[MAX_LINE] = {0};	
	char fnick[MAX_LINE] = {0}; 
	char tuid[MAX_LINE] = {0}; 
	char tios_token[MAX_LINE] = {0}; 
	char qids[MAX_LINE] = {0};
	char *pmessage = NULL;

	while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {

		char *psed = (char *)memchr(pbuf, ' ', strlen(pbuf));
		if (psed != NULL) {
			*psed = '\0';
			int m = (psed - pbuf);

			// 分段取变量
			log_debug("ptok:[%s]", pbuf);

			if (strncasecmp(pbuf, "mid:", 4) == 0) {
				r = snprintf(mid, sizeof(mid), "%s", pbuf+4);
				log_debug("from mid:%d:%s", r, mid);

			} else if (strncasecmp(pbuf, "fuid:", 5) == 0) {
				r = snprintf(fuid, sizeof(fuid), "%s", pbuf+5);
				log_debug("from uid:%d:%s", r, fuid);	

			} else if (strncasecmp(pbuf, "fios_token:", 11) == 0) {
				r = snprintf(fios_token, sizeof(fios_token), "%s", pbuf+11);
				log_debug("from ios_token:%d:%s", r, fios_token);

			} else if (strncasecmp(pbuf, "fnick:", 6) == 0) {
				r = snprintf(fnick, sizeof(fnick), "%s", pbuf+6);
				log_debug("from fnick:%d:%s", r, fnick);

			} else if (strncasecmp(pbuf, "tuid:", 5) == 0) {
				r = snprintf(tuid, sizeof(tuid), "%s", pbuf+5);
				log_debug("from tuid:%d:%s", r, tuid);

			} else if (strncasecmp(pbuf, "tios_token:", 11) == 0) {
				r = snprintf(tios_token, sizeof(tios_token), "%s", pbuf+11);
				log_debug("from tios_token:%d:%s", r, tios_token);

			} else if (strncasecmp(pbuf, "qids:", 5) == 0) {
				r = snprintf(qids, sizeof(qids), "%s", pbuf+5);
				log_debug("from qids:%d:%s", r, qids);

			} 

			pbuf = psed + 1;
			n -= (m + 1);

		} else {
			if (pbuf != NULL) {
				if (strncasecmp(pbuf, "qids:", 5) == 0) {
					pmessage = pbuf + 5;
					n -= 5;
					log_debug("from Pqids:%d:%s", n, pmessage);
				}
			}

			break;
		}

	}


	// 写消息到队列
	char queue_file[MAX_LINE] = {0};
	n = write_content_to_file_with_uid_mfile(&config_st, tuid, pmessage, n, session->mfp_in, queue_file, sizeof(queue_file));
	if (n == -1) {
		log_error("write_content_to_file_with_uid fail");
		goto TXT_FAIL;
	}

	// 写消息索引到数据库
	unsigned long queue_size = n;
    struct stat queue_st;
    if ( lstat(queue_file, &queue_st) == 0 )
        queue_size = queue_st.st_size;

	int qid = write_queue_to_db(&config_st, TAG_RECVSCMTXT, mid, fuid, fnick, fios_token, tuid, tios_token, m_type, queue_file, queue_size);
	if (qid == -1) {
		goto TXT_FAIL;
	}

	// 发送通知
	// ...

	// 检查收件人是否在线
	char mc_value[MAX_LINE] = {0};
	int ret = get_to_mc(config_st.mc_server, config_st.mc_port, atoi(config_st.mc_timeout), tuid, mc_value, sizeof(mc_value));	
	if (ret == 0) {

		/*char *pios_token = NULL;
		char tpid[MAX_LINE] = {0};
		int i = 0;
		while ( (mc_value[i] != ',') && (i < strlen(mc_value)) && (i < sizeof(tpid)) ) {
			tpid[i] = mc_value[i];
			i++;
		}
		i++;

		if ( (i < strlen(mc_value)) && ((pios_token = mc_value + i) != '\0') ) {*/
		if ( *mc_value != '\0' ) {
			char *tpid = mc_value;

			// 收件人在线，使用socket发送通知
			log_debug("tuid:%s is online, use socket send message", tuid);
			n = send_msg_to_another_process(session->parent_out_fd, tpid, fuid, fnick, m_type, tuid);
			if (n == 0) {
				goto TXT_SUCC;
			}

			log_error("use send_socket_cmd send notice fail, use APN send");
			
		}


	}

	// 收件人不在线，使用APN通知
	log_debug("tuid:%s not online, use APNS send msg", tuid);
	//send_apn_cmd(tios_token, fuid, fnick, m_type, tuid);
	send_apn_cmd(&config_st, fuid, fnick, m_type, tuid);
	

	goto TXT_SUCC;



TXT_FAIL:
	if (strlen(queue_file) > 0) {
		unlink(queue_file);
	}

	return 1;

TXT_SUCC:
	return 0;

}


// GET / HTTP/1.1\r\n
// Host: 122.10.119.33:5026\r\n
// User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.9; rv:39.0) Gecko/20100101 Firefox/39.0 FirePHP/0.7.4\r\n
// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n
// Accept-Language: en-US,en;q=0.5\r\n
// Accept-Encoding: gzip, deflate\r\n
// Sec-WebSocket-Version: 13\r\n
// Origin: http://monitor.tjsinamail.com\r\n
// Sec-WebSocket-Extensions: permessage-deflate\r\n
// Sec-WebSocket-Key: +sn+Is+Mv/+J9tKfQOmzbA==\r\n
// x-insight: activate\r\n
// Connection: keep-alive, Upgrade\r\n
// Pragma: no-cache\r\n
// Cache-Control: no-cach\r\n
// Upgrade: websocket\r\n\r
int webSocket_cmd(struct session *session, char *pbuf, size_t pbuf_size, char *websocket_accept_val, size_t websocket_accept_val_size)
{
	log_debug("pbuf:%d:%s", pbuf_size, pbuf);
	
	int r = 0;
	int n = pbuf_size;
	char upgrade[MAX_LINE] = {0};
	char Sec_WebSocket_Key[MAX_LINE] = {0};
	char Sec_WebSocket_Accept[MAX_LINE] = {0};
	
	while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {
		char *psed = (char *)strstr(pbuf, "\r\n");
		if (psed != NULL) {
			*psed = '\0';
			int m = (psed - pbuf);
			
			// 分段取变量
			log_debug("ptok:[%s]", pbuf);
			
			if (strncasecmp(pbuf, "Upgrade: ", 9) == 0) {
				r = snprintf(upgrade, sizeof(upgrade), "%s", pbuf + 9);
				log_debug("from upgrade:%d:%s", r, upgrade);
				
			} else if (strncasecmp(pbuf, "Sec-WebSocket-Key: ", 19) == 0) {
				r = snprintf(Sec_WebSocket_Key, sizeof(Sec_WebSocket_Key), "%s", pbuf + 19);
				log_debug("from Sec_WebSocket_Key:%d:%s", r, Sec_WebSocket_Key);
				
			}
			
			pbuf = psed + 2;
			n -= (m + 2);
			
		} else {
		
			break;
		}
		
	}
	
	r = snprintf(Sec_WebSocket_Accept, sizeof(Sec_WebSocket_Accept), "%s%s", Sec_WebSocket_Key, WebSocketAcceptKey);
	log_debug("Sec_WebSocket_Accept:%s", Sec_WebSocket_Accept);
	
	// sha1 encode
	unsigned char dst[SHA_DIGEST_LENGTH] = {0};
	size_t dst_len = sha1_encode(Sec_WebSocket_Accept, r, dst);
	if (dst_len <= 0) {
		log_error("sha1_encode fail");
        goto WebSocket_FAIL;	
	}

	unsigned char dst_str[SHA_DIGEST_LENGTH*3] = {0};
	int i=0;
	for (i=0; i<SHA_DIGEST_LENGTH; i++) {
		log_debug("dst:%02x", dst[i]);
		snprintf(dst_str+(i*2), 3, "%02x", dst[i]);
	}
	dst_str[40] = '\0';
	log_debug("dst_str:%d:%s", strlen(dst_str), dst_str);
	
	//char Sec_WebSocket_Accept_b64[MAX_LINE] = {0};
	
	// base64 encode
	int Sec_WebSocket_Accept_n = base64_encode(dst, sizeof(dst), websocket_accept_val, websocket_accept_val_size);
	if(Sec_WebSocket_Accept_n <= 0) {
		log_error("base64_decode fail");
        goto WebSocket_FAIL;
	}
	log_debug("Sec_WebSocket_Accept_b64:%d:%s", Sec_WebSocket_Accept_n, websocket_accept_val);
	
WebSocket_FAIL:
	return 0;

WebSocket_SUCC:

	return Sec_WebSocket_Accept_n;	
}



void command(struct session *session)
{
	char fbuf[BUF_SIZE] = {0};
	memset(fbuf, 0, sizeof(fbuf));

	mseek_pos(session->mfp_in, 0);
	int n = mread(session->mfp_in, fbuf, sizeof(fbuf) - 128);	
	if (n <= 0)
		return;

	// 去掉最后的 \0
	//n--;
	log_debug("%d:%s", n, fbuf);

	char rspsoneBuf[512] = {0};
	
	// SENDNOTICETOOTHERPROGRESS: 客户端给直接给别一进程发通知
	if ( strncasecmp(fbuf, "SENDNOTICETOOTHERPROGRESS ", 26) == 0 ) { 
		char *pbuf = fbuf + 26;
		n -= 26;
		int ret = send_notice_to_other_progress(session, pbuf, n);
		if (ret == 0) {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s OK %s", TAG_SNTOP, DATA_END);
		} else {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s FAIL%s", TAG_SNTOP, DATA_END);
		}
		
		if (session->client_type == 1) {
			send_to_websocket(session->client_fd, rspsoneBuf, n);
		} else {
			send_to_socket(session->client_fd, rspsoneBuf, n);
		}

		return;
	}

	// ALIVE: keep alive 保持在线
	if ( strncasecmp(fbuf, "ALIVE ", 6) == 0 ) {
		char *pbuf = fbuf + 6;
		n -= 6;

		n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s OK %s", TAG_ALIVE, DATA_END);	
		if (session->client_type == 1) {
			send_to_websocket(session->client_fd, rspsoneBuf, n);
		} else {
			send_to_socket(session->client_fd, rspsoneBuf, n);
		}

		// 更新超时时间
		time(&g_last_alive_time);

		return;	
	}

	// QUIT: 用户关闭连接
	if ( strncasecmp(fbuf, "QUIT ", 5) == 0 ) {
		char *pbuf = fbuf + 5;
		n -= 5;
		int ret = quit_cmd(session, pbuf, n);
		if (ret == 0) {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s OK %s", TAG_QUIT, DATA_END);
		} else {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s FAIL%s", TAG_QUIT, DATA_END);
		}	

		if (session->client_type == 1) {
			send_to_websocket(session->client_fd, rspsoneBuf, n);
		} else {
			send_to_socket(session->client_fd, rspsoneBuf, n);
		}

		return;
	}

	
	// HELO: 用于表示客户端信息 helo uid:0000001 ios_token:xxxxxxxxxxxxxx fid:0000220
	if ( strncasecmp(fbuf, "helo ", 5) == 0 ) {
		char *pbuf = fbuf + 5;	
		n -= 5;
		//char myuid[128] = {0};
		memset(myuid, '\0', sizeof(myuid));
		int ret = helo_cmd(session, pbuf, n, myuid, sizeof(myuid));
		if (ret == 0) {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s OK %s", TAG_HELO, DATA_END);
		} else {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s FAIL%s", TAG_HELO, DATA_END);
		}

		if (session->client_type == 1) {
			send_to_websocket(session->client_fd, rspsoneBuf, n);
		} else {
			send_to_socket(session->client_fd, rspsoneBuf, n);
		}

		return;
	}

	// SENDADDFRDREQ: 发送请求加为好友信息
	if ( strncasecmp(fbuf, "SENDADDFRDREQ ", 14) == 0 ) {
		char *pbuf = fbuf + 14;
		n -= 14;
		char mid[MAX_LINE] = {0};
		int qid;
		int ret = sendreq_addfriend(session, pbuf, n, mid, sizeof(mid), &qid);
		if (ret == 0) {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s OK %d %s%s", TAG_REQMAKEFRIENDRES, qid, mid, DATA_END);
		} else {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s FAIL %d %s%s", TAG_REQMAKEFRIENDRES, qid, mid, DATA_END);
		}

		if (session->client_type == 1) {
			send_to_websocket(session->client_fd, rspsoneBuf, n);
		} else {
			send_to_socket(session->client_fd, rspsoneBuf, n);
		}

		return;
	}

	// SENDNOTICE: 发送通知对方收消息信息
	if ( strncasecmp(fbuf, "SENDGETNEWMSG ", 13) == 0 ) {
		char *pbuf = fbuf + 13;
		n -= 13;
		char mid[MAX_LINE] = {0};
		int ret = sendget_newmsg(session, pbuf, n, mid, sizeof(mid));
		if (ret == 0) {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s OK %s%s", TAG_SENDGETNEWMSG, mid, DATA_END);

		} else {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s FAIL %s%s", TAG_SENDGETNEWMSG, mid, DATA_END);

		}

		if (session->client_type == 1) {
			send_to_websocket(session->client_fd, rspsoneBuf, n);
		} else  {
			send_to_socket(session->client_fd, rspsoneBuf, n);
		}

		return;
	}

	// SENDTXT: 发送送信息给对方
	if ( strncasecmp(fbuf, "SENDTXT ", 8) == 0 ) {
		char *pbuf = fbuf + 8;
		n -= 8;
		char mid[MAX_LINE] = {0};
		char qid[MAX_LINE] = {0};
		int ret = sendtxt_cmd(session, pbuf, n, mid, sizeof(mid), qid, sizeof(qid));
		if (ret == 0) {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s OK %s %s%s", TAG_SENDTXT, mid, qid, DATA_END);
		} else {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s FAIL %s -1%s", TAG_SENDTXT, mid, DATA_END);

		}

		if (session->client_type == 1) {
			send_to_websocket(session->client_fd, rspsoneBuf, n);
		} else {
			send_to_socket(session->client_fd, rspsoneBuf, n);
		}

		return;
	}

	// SENDIMG: 发送送图片给对方
	if ( strncasecmp(fbuf, "SENDIMG ", 8) == 0 ) {
		char *pbuf = fbuf + 8;
		n -= 8;

		char mid[MAX_LINE] = {0};
		char thumb_img_url[MAX_LINE] = {0};
		char qid[MAX_LINE] = {0};
		int ret = sendimg_cmd(session, pbuf, n, mid, sizeof(mid), thumb_img_url, sizeof(thumb_img_url), qid, sizeof(qid));
		if (ret == 0) {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s OK %s %s %s%s", TAG_SENDIMG, mid, qid, thumb_img_url, DATA_END); 			
		} else {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s FAIL %s %s %s%s", TAG_SENDIMG, mid, qid, thumb_img_url, DATA_END);
		}

		if (session->client_type == 1) {
			send_to_websocket(session->client_fd, rspsoneBuf, n);
		} else {
			send_to_socket(session->client_fd, rspsoneBuf, n);
		}

		return ;
	}

	// SENDAUDIO: 发送送音频给对方
	if ( strncasecmp(fbuf, "SENDAUDIO ", 10) == 0 ) {
		char *pbuf = fbuf + 10;
		n -= 10;

		char mid[MAX_LINE] = {0};
		char qid[MAX_LINE] = {0};
		char audio_url[MAX_LINE] = {0};
		int ret = sendaudio_cmd(session, pbuf, n, mid, sizeof(mid), audio_url, sizeof(audio_url), qid, sizeof(qid));
		if (ret == 0) {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s OK %s %s %s %s", TAG_SENDAUDIO, mid, qid, audio_url, DATA_END);
		} else {
			n = snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%s FAIL %s %s %s %s", TAG_SENDAUDIO, mid, qid, audio_url, DATA_END);
		}

		if (session->client_type == 1) {
			send_to_websocket(session->client_fd, rspsoneBuf, n);
		} else {
			send_to_socket(session->client_fd, rspsoneBuf, n);
		}

		return;
	}
	
	// DELETESCMData: 通知对方删除消息
	if ( strncasecmp(fbuf, "DELETESCMData ", 14) == 0 ) {
		char *pbuf = fbuf + 14;
		n -= 14;
		
		deleteSCMData_cmd(session, pbuf, n);	

		return;
	}


	// WebSocket
	if ( strncasecmp(fbuf, "GET / HTTP/1.1\r\n", 16) == 0) {
		char *pbuf = fbuf + 16;
		n -= 16;
		
		session->client_type = 1;

		char WebSocket_Response[MAX_LINE] = {0};
		
		char websocket_accept_val[MAX_LINE] = {0};
		int ret = webSocket_cmd(session, pbuf, n, websocket_accept_val, sizeof(websocket_accept_val));
		
		memset(WebSocket_Response, 0, sizeof(WebSocket_Response));

		sprintf(WebSocket_Response, "HTTP/1.1 101 Switching Protocols\r\n");
		sprintf(WebSocket_Response, "%sUpgrade: websocket\r\n", WebSocket_Response);
		sprintf(WebSocket_Response, "%sConnection: Upgrade\r\n", WebSocket_Response);
		sprintf(WebSocket_Response, "%sSec-WebSocket-Accept: %s\r\n\r\n", WebSocket_Response, websocket_accept_val);

		log_debug("response:%d:%s", strlen(WebSocket_Response), WebSocket_Response);

		send_to_socket(session->client_fd, WebSocket_Response, strlen(WebSocket_Response));

		return;
	}
}


void system_command(MFILE *mfp_parent)
{
	char fbuf[BUF_SIZE] = {0};
	int n = mread_line(mfp_parent, fbuf, sizeof(fbuf));	
	if (n > 0) {
		log_debug("%d:%s", n, fbuf);

		// type:SSYSTEM_SENDMSG pid:18960 fuid:18 fnick:5a6L5YGl mtype:sys tuid:19
		if ( strncasecmp(fbuf, "type:SSYSTEM_SENDMSG ", 21) == 0 ) {
			// 内部通信
			char cmd_pid[MAX_LINE] = {0}; 
			char cmd_mqid[MAX_LINE] = {0}; 
			char cmd_fuser_type[MAX_LINE] = {0}; 
			char cmd_fuid[MAX_LINE] = {0}; 
			char cmd_fnick[MAX_LINE] = {0}; 
			char cmd_mtype[MAX_LINE] = {0}; 
			char cmd_tuid[MAX_LINE] = {0}; 

			n -= 21;
			int r = 0;     
    		int buf_len = n;
			char *pbuf = fbuf + 21;

			while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {
				char *psed = (char *)memchr(pbuf, ' ', n);
				if (psed != NULL) {
					*psed = '\0';
					int m = (psed - pbuf);
					if (m > 0) { 
						// 分段取变量
						log_debug("ptok:[%s]", pbuf);
     
						if (strncasecmp(pbuf, "pid:", 4) == 0) {
							r = snprintf(cmd_pid, sizeof(cmd_pid), "%s", pbuf+4);
							log_debug("from cmd_pid:%d:%s", r, cmd_pid);

						} else if (strncasecmp(pbuf, "mqid:", 5) == 0) {
							r = snprintf(cmd_mqid, sizeof(cmd_mqid), "%s", pbuf+5);
							log_debug("from cmd_mqid:%d:%s", r, cmd_mqid);

						} else if (strncasecmp(pbuf, "fuser_type:", 11) == 0) {
							r = snprintf(cmd_fuser_type, sizeof(cmd_fuser_type), "%s", pbuf+11);
							log_debug("from cmd_fuser_type:%d:%s", r, cmd_fuser_type);

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
							while ( cmd_tuid[r - 1] == '\n' )
								r--;
							cmd_tuid[r - 1] = '\0';
							log_debug("from cmd_tuid:%d:%s", r, cmd_tuid);

						}
					}
					break;
				}
			}


			// send socket to client
			//int succ = send_socket_cmd(session_st.client_fd, session_st.client_type, cmd_fuid, cmd_fnick, cmd_mtype, cmd_tuid);
			int succ = send_socket_cmd_v2(session_st.client_fd, session_st.client_type, cmd_mqid, cmd_fuser_type, cmd_fuid, cmd_fnick, cmd_mtype, cmd_tuid);
			log_debug("send_socket_cmd to client socket fd:%d result:%d", session_st.client_fd, succ);

		} else if ( strncasecmp(fbuf, "type:SSYSTEM_SENDAPNS ", 21) == 0 ) {
			// type:SSYSTEM_SENDAPNS fuid:18 fnick:5a6L5YGl mtype:sys tuid:19
			
			// 使用APNS给tuid发通知
			char cmd_fuid[MAX_LINE] = {0}; 
			char cmd_fnick[MAX_LINE] = {0}; 
			char cmd_mtype[MAX_LINE] = {0}; 
			char cmd_tuid[MAX_LINE] = {0}; 

			n -= 21;
			int r = 0;     
    		int buf_len = n;
			char *pbuf = fbuf + 21;

			while ((n > 0) && (pbuf != NULL) && (*pbuf != '\0')) {
				char *psed = (char *)memchr(pbuf, ' ', n);
				if (psed != NULL) {
					*psed = '\0';
					int m = (psed - pbuf);
					if (m > 0) { 
						// 分段取变量
						log_debug("ptok:[%s]", pbuf);
     
						if (strncasecmp(pbuf, "fuid:", 5) == 0) {
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
							while ( cmd_tuid[r - 1] == '\n' )
								r--;
							cmd_tuid[r - 1] = '\0';
							log_debug("from cmd_tuid:%d:%s", r, cmd_tuid);

						}
					}
					break;
				}
			}
			
			// send apns to client
			//int succ = send_socket_cmd(session_st.client_fd, session_st.client_type, cmd_fuid, cmd_fnick, cmd_mtype, cmd_tuid);
			send_apn_cmd(&config_st, cmd_fuid, cmd_fnick, cmd_mtype, cmd_tuid);
			log_debug("send_apn_cmd to client.");	
			
		}
	}
}




void liao_clean()
{
	if (session_st.mfp_in != NULL) {
		mclose(session_st.mfp_in);
		session_st.mfp_in = NULL;
	}
	if (session_st.mfp_out != NULL) {
		mclose(session_st.mfp_out);
		session_st.mfp_out = NULL;
	}
	session_st.client_type = 0;
	session_st.client_fd = -1;
	session_st.parent_in_fd = -1;
	session_st.parent_out_fd = -1;

	memset(session_st.uid, 0, sizeof(session_st.uid));
	memset(session_st.ios_token, 0, sizeof(session_st.ios_token));

	if (epoll_fdc > 0) {
		close(epoll_fdc);
		epoll_fdc = -1;
	}

}


void child_quit()
{
	int sockfd = session_st.client_fd;
	if (*(session_st.uid) != '\0') {
	
		// 注册为离线   (uid => ios_token)
		int sockfd = session_st.client_fd;
		int ret = delete_from_mc(config_st.mc_server, config_st.mc_port, atoi(config_st.mc_timeout), session_st.uid);
		if (ret != 0) {
			log_error("delete from online failed uid[%s] sockfd[%d] ios_token[%s]", session_st.uid, sockfd, session_st.ios_token);
			return;

		}
		log_debug("offline fuid:%s", session_st.uid);
	}

    close(sockfd);

    _exit(100);
}


void websocket_command(struct session *session)
{
	/*char fbuf[BUF_SIZE] = {0};
	memset(fbuf, 0, sizeof(fbuf));

	mseek_pos(session->mfp_in, 0);
	int n = mread(session->mfp_in, fbuf, sizeof(fbuf) - 128);	
	if (n <= 0)
		return;

	// 去掉最后的 \0
	log_debug("%d:%s", n, fbuf);*/
	
	command(session);
}

void quit(char *prepend, char *append)
{
	char outbuf[1024] = {0};
	int n = snprintf(outbuf, sizeof(outbuf), "%s:%s\n", prepend, append);
	int r = write(1, outbuf, n);
	_exit(0);	
}

void usage(char *prog)
{
	//printf("%s [mid] [client ip] [client port] [log_level] [queue_path] [queue_host]\n", prog);
	printf("%s -m[mid] -c[child config file] -i[client ip] -p[client port]\n", prog);
}


// fd 说明:
//	0: 客户端fd
//	1: 向父进程写fd
//	2: 从父进程读fd
int main(int argc, char **argv)
{
	liao_log("liao_command", LOG_PID|LOG_NDELAY, LOG_MAIL);

	if (argc != 5) {
		usage(argv[0]);
		_exit(100);
	}

	char child_cf[512] = {0};
	char client_ip[20] = {0};		
	char client_port[20] = {0};

	int c;
	const char *args = "c:m:i:p:h";
	while ((c = getopt(argc, argv, args)) != -1) {
		switch (c) {
			case 'm':
				snprintf(msg_id, sizeof(msg_id), "%s", optarg);
				break;
			case 'c':
				snprintf(child_cf, sizeof(child_cf), "%s", optarg);
				break;
			case 'i':
				snprintf(client_ip, sizeof(client_ip), "%s", optarg);
				break;
			case 'p':
				snprintf(client_port, sizeof(client_port), "%s", optarg);			
				break;
			case 'h':
			default:
				usage(argv[0]);
				_exit(111);
		}	
	}

	// 处理配置文件
	if (conf_init(child_cf)) {
        fprintf(stderr, "Unable to read control files:%s\n", child_cf);
        log_error("Unable to read control files:%s", child_cf);
        _exit(111);
    }

	// 检查队列目录
    if (strlen(config_st.queue_path) <= 0) { 
        fprintf(stderr, "Unable to read queue path.\n");
        log_error("Unable to read queue path.");
        _exit(111);
    }    
    if ( access(config_st.queue_path, F_OK) ) {
        fprintf(stderr, "Queue path:%s not exists:%s, so create it.\n", config_st.queue_path, strerror(errno)); 
        log_error("Queue path:%s not exists:%s, so create it.", config_st.queue_path, strerror(errno)); 

        umask(011);
        mkdir(config_st.queue_path, 0777);
    }



	if (strcasecmp(config_st.log_level, "debug") == 0) {
		log_level = debug;
	} else if (strcasecmp(config_st.log_level, "info") == 0) {
		log_level = info;
	}

	// ----
	session_st.client_type = 0;
	session_st.client_fd = 0;
	session_st.parent_out_fd = 1;
	session_st.parent_in_fd = 2;
	memset(session_st.uid, 0, sizeof(session_st.uid));
	memset(session_st.ios_token, 0, sizeof(session_st.ios_token));


	session_st.mfp_in = mopen(MAX_BLOCK_SIZE, NULL, NULL);
	if (session_st.mfp_in == NULL) {
		log_error("mopen fail for mfp_in");
		_exit(100);
	}
	session_st.mfp_out = mopen(MAX_BLOCK_SIZE, NULL, NULL);
	if (session_st.mfp_out == NULL) {
		log_error("mopen fail for mfp_out");
		liao_clean();
		_exit(100)	;
	}

	log_info("get param mid:%s cip:%s cport:%s", msg_id, client_ip, client_port);


	char buf[BUF_SIZE] = {0};
	char tbuf[BUF_SIZE] = {0};
	char *ptbuf = tbuf;
	int tbuf_size = BUF_SIZE;
	int tbuf_len = 0;
	int n = 0;
	char ch;

	memset(tbuf, 0, sizeof(tbuf));
	tbuf_len = 0;
	ptbuf = tbuf + tbuf_len;

	int state = 0;


	// create gretting info
	/*char greetting[512] = {0};
	char hostname[1024] = {0};
	if (gethostname(hostname, sizeof(hostname)) != 0) {
		snprintf(hostname, sizeof(hostname), "unknown");
	}
	n = snprintf(greetting, sizeof(greetting), "%s OK %s %s%s", TAG_GREET, hostname, msg_id, DATA_END);
	if (n > 0) {
		write(session_st.client_fd, greetting, n);	
	}*/


	// 设置超时检查定时器
    set_check_client_timeout();


	// epoll initialize
	int epoll_i = 0;
	epoll_event_num = 4;
	epoll_evts = (struct epoll_event *)malloc(epoll_event_num * sizeof(struct epoll_event));
	if (epoll_evts == NULL) {
		log_error("alloc epoll event failed");
		_exit(111);
	}
	

	// epoll create fd
	epoll_fdc = epoll_create(epoll_event_num);	
	if (epoll_fdc <= 0) {
		log_info("create epoll fd failed.error:%d %s", errno, strerror(errno));
		liao_clean();
		_exit(111);
	}

	// add fd 0 to epoll for send
	if (setnonblocking(session_st.client_fd) != 0) {
		log_error("setnonblocking fd[%d] failed", session_st.client_fd);
	}

	struct epoll_event send_evt;
	send_evt.events = EPOLLIN | EPOLLET;
	//send_evt.events = EPOLLIN;
	send_evt.data.fd = session_st.client_fd;
	if (epoll_ctl(epoll_fdc, EPOLL_CTL_ADD, session_st.client_fd, &send_evt) == -1) {
		log_info("add event to epoll fail. fd:%d event:EPOLLIN", send_evt.data.fd);
		liao_clean();
		_exit(111);
	}
	log_debug("add fd:%d event to epoll succ", send_evt.data.fd);

	// add parent read fd pipe_r to epoll for send
	if (setnonblocking(session_st.parent_in_fd) != 0) {
		log_error("setnonblocking fd[%d] failed", session_st.parent_in_fd);
	}

	struct epoll_event pipe_r_evt;
	pipe_r_evt.events = EPOLLIN | EPOLLET;
	pipe_r_evt.data.fd = session_st.parent_in_fd;
	if (epoll_ctl(epoll_fdc, EPOLL_CTL_ADD, session_st.parent_in_fd, &pipe_r_evt) == -1) {
		log_info("add event to epoll fail. fd:%d event:EPOLLIN",  pipe_r_evt.data.fd);
		liao_clean();
		_exit(111);
	}
	log_debug("add fd:%d event to epoll succ", pipe_r_evt.data.fd);


	while (1) {
		epoll_nfds = epoll_wait(epoll_fdc, epoll_evts, epoll_event_num, -1);
		if (epoll_nfds == -1 ) {
			if (errno == EINTR) // 接收到中断信号
				continue;
			
			_exit(111);
		} else if (epoll_nfds > 0) {
			log_debug("epoll_nfds:%d", epoll_nfds);
			for (epoll_i = 0; epoll_i < epoll_nfds; epoll_i++) {
				int evt_fd = epoll_evts[epoll_i].data.fd;
			
				log_debug("epoll[%d] fd:%d event:%d", epoll_i, evt_fd, epoll_evts[epoll_i].events);

				if ((epoll_evts[epoll_i].events & EPOLLIN) && (evt_fd == session_st.parent_in_fd)) {
					log_debug("get event EPOLLIN from parent socket fd:%d", evt_fd);
					
					// 读取内容 ----------------------
					MFILE *mfp_parent_in = mopen(MAX_BLOCK_SIZE, NULL, NULL);
					if (mfp_parent_in == NULL) {
						log_error("mopen fail for mfp_parent_in");
						continue;	
					}

					memset(buf, 0, sizeof(buf));
					memset(tbuf, 0, sizeof(tbuf));
					tbuf_len = 0;
					ptbuf = tbuf + tbuf_len;
					
					int i = 0;
					n = read(evt_fd, buf, sizeof(buf));
					if (n == 0) 
						continue;


					while (n > 0) {
						while (i < n) {
							
							ch = buf[i];

							if ((tbuf_size - tbuf_len) < 2) {
								int r = fast_write(mfp_parent_in, tbuf, tbuf_len);
								if (r == 0) {
									log_error("fast_write fail");

									mclose(mfp_parent_in);
									mfp_parent_in = NULL;

									goto LIAO_CONTINUE;	
								}

								memset(tbuf, 0, tbuf_size);
								tbuf_len = 0;
							}

							mybyte_copy(ptbuf+tbuf_len, 1, &ch);
							tbuf_len++;

							if (ch == '\n') {
								// 一条指令完成
								if (tbuf_len > 0) {
									int r = fast_write(mfp_parent_in, tbuf, tbuf_len);
									if (r == 0) {
										log_error("fast_write fail");
										
										mclose(mfp_parent_in);
										mfp_parent_in = 0;

										goto LIAO_CONTINUE;
									}

									memset(tbuf, 0, tbuf_size);
									tbuf_len = 0;
								}

								// 处理当前指令
								// ...
								log_debug("---- system command start ----");
								system_command(mfp_parent_in);
								log_debug("---- system command end ----");
								// ...

								// 处理完成后

								if (mfp_parent_in != NULL) {
									mclose(mfp_parent_in);
									mfp_parent_in = NULL;

								}

								memset(tbuf, 0, sizeof(tbuf));
								tbuf_len = 0;
								ptbuf = tbuf + tbuf_len;
							}

							i++;
						}

						break;
					}

				} else if ((epoll_evts[epoll_i].events & EPOLLIN) && (evt_fd == session_st.client_fd)) {
					log_debug("get event EPOLLIN from client socket fd:%d", evt_fd);
			
					// 读取内容 ----------------------
					memset(buf, 0, sizeof(buf));

					int i = 0;
					n = read(evt_fd, buf, sizeof(buf));
					
					if (session_st.client_type == 1) {

						log_debug("read from WebSocket fd:%d n:%d",  evt_fd, n);
						
						char payload_data[MAX_BLOCK_SIZE];
						int ws_ret = websocket_fmt_data_decode(buf, n, payload_data, sizeof(payload_data));
						if (ws_ret == 3) {
							// error
							continue;
						} else if (ws_ret == 4) {
							// ping
							continue;
						} else if (ws_ret == 2) {
							// close
							log_debug("client close fd:%d", evt_fd);	

							// 注册为离线   (uid => ios_token)
							int ret = delete_from_mc(config_st.mc_server, config_st.mc_port, atoi(config_st.mc_timeout), session_st.uid);
							if (ret != 0) { 
								log_error("delete from online failed uid[%s] sockfd[%d] ios_token[%s]", session_st.uid, evt_fd, session_st.ios_token);   

							}    
							log_debug("offline fuid:%s", session_st.uid);

							send_to_socket(1, "K 250 OK\n", 9);
			
							struct epoll_event ev;
							ev.data.fd = evt_fd;
							if (epoll_ctl(epoll_fdc, EPOLL_CTL_DEL, evt_fd, 0) == -1) {
								log_error("epoll_ctl fd[%d] EPOLL_CTL_DEL failed:[%d]%s", evt_fd, errno, strerror(errno));
							}		

							close(evt_fd);
							log_debug("sockfd:%d closed", evt_fd);

							_exit(100);
							
						} else if (ws_ret == 1) {
							// no finish
							
							continue;
						}
						
						int r = fast_write(session_st.mfp_in, payload_data, strlen(payload_data));
						if (r == 0) {
							log_error("fast_write fail");

							goto LIAO_RESETCONTINUE;	
						}
						
						// 处理当前指令
						// ...
						log_debug("---- websocket command start ----");
						websocket_command(&session_st);
						log_debug("---- websocket command end ----");
						// ...
						
						// 处理完成后
						if (session_st.mfp_in != NULL) {
							mclose(session_st.mfp_in);
							session_st.mfp_in = NULL;

							session_st.mfp_in = mopen(MAX_BLOCK_SIZE, NULL, NULL);
							if (session_st.mfp_in == NULL) {
								log_error("mopen fail for mfp_out");
								liao_clean();
								_exit(100)  ;
							}  
						}
					
						continue;
					}
					
					log_debug("read fd:%d n:%d:%s", evt_fd, n, buf);
					if (n == 0) {
						// client close;
						log_debug("client close fd:%d", evt_fd);	

						// 注册为离线   (uid => ios_token)
						int ret = delete_from_mc(config_st.mc_server, config_st.mc_port, atoi(config_st.mc_timeout), session_st.uid);
						if (ret != 0) { 
							log_error("delete from online failed uid[%s] sockfd[%d] ios_token[%s]", session_st.uid, evt_fd, session_st.ios_token);   

						}    
						log_debug("offline fuid:%s", session_st.uid);

						send_to_socket(1, "K 250 OK\n", 9);
			
						struct epoll_event ev;
						ev.data.fd = evt_fd;
						if (epoll_ctl(epoll_fdc, EPOLL_CTL_DEL, evt_fd, 0) == -1) {
							log_error("epoll_ctl fd[%d] EPOLL_CTL_DEL failed:[%d]%s", evt_fd, errno, strerror(errno));
						}		

						close(evt_fd);
						log_debug("sockfd:%d closed", evt_fd);

						_exit(100);
					}					

					log_info("buf[0]:%d buf[1]:%d", buf[0], buf[1]);
					//int is_gzip = is_GZippedData(buf, n);			
					//log_info("is_gzip:%d", is_gzip);

					log_debug("read socket:%d data:%d:%s", evt_fd, n, buf);
					while (n > 0) {
						while (i < n) {
							ch = buf[i];
							if (ch == '\r' || ch == '\n') {
								state++;
							} else {
								state = 0;
							}

							if ((tbuf_size - tbuf_len) < 2) {
								int r = fast_write(session_st.mfp_in, tbuf, tbuf_len);
								log_debug("fast_write 1: r:%d tbuf_len:%d", r, tbuf_len);
								if (r == 0) {
									log_error("fast_write fail");

									char rspsoneBuf[MAX_LINE] = {0};
									snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%ld SYS fail%s", TAG_SENDTXT, DATA_END);	
									write(session_st.client_fd, rspsoneBuf, strlen(rspsoneBuf));

									goto LIAO_RESETCONTINUE;	
								}

								memset(tbuf, 0, tbuf_size);
								tbuf_len = 0;
								ptbuf = tbuf + tbuf_len;
							}

							mybyte_copy(ptbuf+tbuf_len, 1, &ch);
							tbuf_len++;

							if (state == 4) {
								// 一条指令完成
								if (tbuf_len > 0) {
									int r = fast_write(session_st.mfp_in, tbuf, tbuf_len);
									log_debug("fast_write 2: r:%d tbuf_len:%d", r, tbuf_len);
									if (r == 0) {
										log_error("fast_write fail");

										char rspsoneBuf[MAX_LINE] = {0};
										snprintf(rspsoneBuf, sizeof(rspsoneBuf), "%ld SYS fail%s", TAG_SENDTXT, DATA_END);
										write(session_st.client_fd, rspsoneBuf, strlen(rspsoneBuf));

										goto LIAO_RESETCONTINUE;
									}

									memset(tbuf, 0, tbuf_size);
									tbuf_len = 0;
									ptbuf = tbuf + tbuf_len;
								}

								// 处理当前指令
								// ...
								log_debug("---- command start ----");
								command(&session_st);
								log_debug("---- command end ----");
								// ...

								// 处理完成后
								//goto LIAO_RESETCONTINUE;
								state = 0;

								if (session_st.mfp_in != NULL) {
									mclose(session_st.mfp_in);
									session_st.mfp_in = NULL;

									session_st.mfp_in = mopen(MAX_BLOCK_SIZE, NULL, NULL);
									if (session_st.mfp_in == NULL) {
										log_error("mopen fail for mfp_out");
										liao_clean();
										_exit(100)  ;
									}  
								}

							}

							i++;
						}	

						memset(buf, 0, sizeof(buf));
						n = read(evt_fd, buf, sizeof(buf));
						log_debug("read socket:%d data:%d:%s", evt_fd, n, buf);
						i = 0;
					}

				} else if (epoll_evts[epoll_i].events & EPOLLOUT) {
					log_debug("get event EPOLLOUT socket fd:%d", evt_fd);

				} else if (epoll_evts[epoll_i].events & EPOLLHUP) {
					log_debug("get event EPOLLHUP socket fd:%d", evt_fd);

				}
LIAO_CONTINUE:
				continue;


LIAO_RESETCONTINUE:
				log_error("LIAO_RESETCONTINUE");
				if (session_st.mfp_in != NULL) {
					mclose(session_st.mfp_in);
					session_st.mfp_in = NULL;
				}
				session_st.mfp_in = mopen(MAX_BLOCK_SIZE, NULL, NULL);
				if (session_st.mfp_in == NULL) {
        			log_error("mopen fail for mfp_out");
        			liao_clean();
        			_exit(100)  ;
    			}  

				continue;

			}
		}
	}



	// 写给客户端
	//write(0, "greeting ok\r\n", 13);

	// 写给父进程 
	//write(1, "K 250 OK\n", 9);

	return 0;
}





