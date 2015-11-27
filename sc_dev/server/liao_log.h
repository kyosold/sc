#ifndef _LIAO_LOG_H
#define _LIAO_LOG_H

#include "syslog.h"

extern int log_level;
extern char msg_id[100];

#define debug   8
#define info    7   
#define notice	6
#define warning	5
#define	error	4
#define	crit	3
#define	alert	2
#define	emerg	1


#define log_emerg(fmt, ...) syslog(LOG_EMERG, "[EMERG] %s %s "fmt, __func__, msg_id, ##__VA_ARGS__)
#define log_alert(fmt, ...) syslog(LOG_ALERT, "[ALERT] %s %s "fmt, __func__, msg_id, ##__VA_ARGS__)
#define log_crit(fmt, ...) syslog(LOG_CRIT, "[CRIT] %s %s "fmt, __func__, msg_id, ##__VA_ARGS__)
#define log_error(fmt, ...) syslog(LOG_ERR, "[ERROR] %s %s "fmt, __func__, msg_id, ##__VA_ARGS__)
#define log_warning(fmt, ...) syslog(LOG_WARNING, "[WARNING] %s %s "fmt, __func__, msg_id, ##__VA_ARGS__)
#define log_notice(fmt, ...) syslog(LOG_NOTICE, "[NOTICE] %s %s "fmt, __func__, msg_id, ##__VA_ARGS__)
#define log_info(fmt, ...) {if(log_level>=info){syslog(LOG_INFO, "[INFO] %s %s "fmt, __func__, msg_id, ##__VA_ARGS__);}}
#define log_debug(fmt, ...) {if(log_level>=debug){syslog(LOG_DEBUG, "[DEBUG] %s %s "fmt, __func__, msg_id, ##__VA_ARGS__);}}

void liao_log(const char *ident, int option, int facility);

#endif
