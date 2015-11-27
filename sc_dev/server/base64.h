/*
**  Copyright (c) 2005 Sendmail, Inc. and its suppliers.
**    All rights reserved.
**
**  Copyright (c) 2009, The OpenDKIM Project.  All rights reserved.
*/

#ifndef _BASE64_H_
#define _BASE64_H_


/* system includes */
#include <sys/types.h>

/* prototypes */
extern int base64_decode(u_char *str, u_char *buf, size_t buflen);
extern int base64_encode(u_char *data, size_t datalen, u_char *buf,
                              size_t buflen);


/* SJ add */
#include <stdio.h>
#include "mfile.h"
#define BUF_SIZE        8192
extern int sj_b64_decode(MFILE *mfp_in, char *pre_buf, int pre_buf_size, FILE *fp);


#endif /* ! _BASE64_H_ */
