#ifndef _LIAO_COMMANDS_H
#define _LIAO_COMMANDS_H


#define MAX_LINE        1024
#define BUF_SIZE        8192
#define MAX_BLOCK_SIZE  262144
#define CFG_FILE        "./liao_config.ini"
#define DEF_CLIENTTIMEOUT	"300"

#define TAG_GREET       "GREET"
#define TAG_HELO        "HELO"
#define TAG_ALIVE       "ALIVE"
#define TAG_LOGIN       "LOGIN"
#define TAG_SENDTXT     "SENDTXT"
#define TAG_RECVTXT     "RECVTXT"
#define TAG_RECVSCMTXT	"RECVSCMTXT"
#define TAG_SENDIMG     "SENDIMG"
#define TAG_RECVIMG     "RECVIMG"
#define TAG_SENDAUDIO   "SENDAUDIO"
#define TAG_RECVAUDIO   "RECVAUDIO"
#define TAG_REQMAKEFRIENDRES "REQMAKEFRIENDRES"
#define TAG_RECVADDFRIENDREQ "RECVADDFRDREQ"
#define TAG_SENDGETNEWMSG "SENDGETNEWMSG"
#define TAG_QUIT        "QUIT"

#define CRLF          (u_char *) "\r\n"
#define DATA_END        (u_char *) "\r\n"

#define API_HTTP        "http://sc.icarot.com/api"
#define IMG_PATH        "/data1/htdocs/sc/data/"
#define IMG_HOST        "http://sc.icarot.com/data/"

#define DEF_LOGLEVEL    "info"
#define DEF_MCSERVER    "127.0.0.1"
#define DEF_MCPORT      "11211"
#define DEF_MCTIMEOUT   "5000"
#define DEF_MAXWORKS    "512"
#define DEF_QUEUEPATH   "/data1/htdocs/sc/data/"
#define DEF_QUEUEHOST   "http://sc.icarot.com//data/"

#define DEF_MYSQLSERVER "localhost"
#define DEF_MYSQLPORT   "3306"
#define DEF_MYSQLUSER   "admin"
#define DEF_MYSQLPASSWD "1234qwer"
#define DEF_MYSQLDB     "liao"
#define DEF_MYSQLTIMEOUT    "2"

#define OFFLINE_MSG_CONTENT "content.txt"
#define OFFLINE_MSG_CONENT_IDX  "content_idx.txt"

#define THUMB_SIZE_W    139
#define THUMB_SIZE_H    139

#define WebSocketAcceptKey "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static char fb_pid[8192] = {0};


struct config_t
{
	char log_level[512];
	int client_timeout;

	// ---- queue ----
	char queue_path[512];
    char queue_host[512];

	// ---- mc -----
    char use_mc[10];
    char mc_server[512];
    int mc_port;
    char mc_timeout[512];

	// ---- mysql ----
    char mysql_host[512];
    int mysql_port;
    char mysql_user[512];
    char mysql_passwd[512];
    char mysql_db[512];
    char mysql_timeout[512];
};

struct session
{
	int client_type;	// 0: app	2: web browse
    int client_fd;
    int parent_in_fd;
    int parent_out_fd;

    char uid[MAX_LINE];
    char ios_token[MAX_LINE];
    
    MFILE *mfp_in;
    MFILE *mfp_out;
};


//void commands(struct clients *client_t);
//void status_cmd(int sockfd, char *pbuf, size_t pbuf_size);
//int helo_cmd(int sockfd, char *pbuf, size_t pbuf_size, char *myuid, size_t myuid_size);
//int sendtxt_cmd(int sockfd, char *pbuf, size_t pbuf_size);
//int sendimg_cmd(int sockfd, char *pbuf, size_t pbuf_size);

//int login_cmd(struct clients *client_t, char *pbuf, size_t pbuf_size);
//int helo_cmd(int sockfd, char *pbuf, size_t pbuf_size, char *myuid, size_t myuid_size);
//int sendtxt_cmd(int sockfd, char *pbuf, size_t pbuf_size, char *mid, size_t mid_size, int client_idx);
//int sendimg_cmd(int sockfd, char *pbuf, size_t pbuf_size, char *mid, size_t mid_size, int client_idx, char *img_full_path, size_t img_full_path_size, char *img_name, size_t img_name_size);
//int quit_cmd(int sockfd, char *pbuf, size_t pbuf_size);

//int get_offline_msg(int sockfd, char *pbuf, size_t pbuf_size);

#endif
