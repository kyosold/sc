/*
**  Copyright (c) 2005, 2008 Sendmail, Inc. and its suppliers.
**    All rights reserved.
**
**  Copyright (c) 2009, The OpenDKIM Project.  All rights reserved.
*/

/* system includes */
#include <sys/types.h>
#include <assert.h>

/* libopendkim includes */
#include "base64.h"
#include "liao_log.h"

/* base64 alphabet */
static unsigned char alphabet[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* base64 decode stuff */
static int decoder[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 62, 0, 0, 0, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0,
	0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
	14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 0,
	0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#ifndef NULL
# define NULL	0
#endif /* ! NULL */

/*
**  DKIM_BASE64_DECODE -- decode a base64 blob
**
**  Parameters:
**  	str -- string to decide
**  	buf -- where to write it
**  	buflen -- bytes available at "buf"
**
**  Return value:
**  	>= 0 -- success; length of what was decoded is returned
**  	-1 -- corrupt
**  	-2 -- not enough space at "buf"
*/

int
base64_decode(u_char *str, u_char *buf, size_t buflen)
{
	int n = 0;
	int bits = 0;
	int char_count = 0;
	u_char *c;

	assert(str != NULL);
	assert(buf != NULL);

	for (c = str; *c != '=' && *c != '\0'; c++)
	{
		/* end padding */
		if (*c == '=' || *c == '\0')
			break;

		/* skip stuff not part of the base64 alphabet (RFC2045) */
		if (!((*c >= 'A' && *c <= 'Z') ||
		      (*c >= 'a' && *c <= 'z') ||
		      (*c >= '0' && *c <= '9') ||
		      (*c == '+') ||
		      (*c == '/')))
			continue;

		/* everything else gets decoded */
		bits += decoder[(int) *c];
		char_count++;
		if (n + 3 > buflen)
			return -2;
		if (char_count == 4)
		{
			buf[n++] = (bits >> 16);
			buf[n++] = ((bits >> 8) & 0xff);
			buf[n++] = (bits & 0xff);
			bits = 0;
			char_count = 0;
		}
		else
		{
			bits <<= 6;
		}
	}

	/* XXX -- don't bother checking for proper termination (for now) */

	/* process trailing data, if any */
	switch (char_count)
	{
	  case 0:
		break;

	  case 1:
		/* base64 decoding incomplete; at least two bits missing */
		return -1;

	  case 2:
		if (n + 1 > buflen)
			return -2;
		buf[n++] = (bits >> 10);
		break;

	  case 3:
		if (n + 2 > buflen)
			return -2;
		buf[n++] = (bits >> 16);
		buf[n++] = ((bits >> 8) & 0xff);
		break;
	}

	return n;
}

/*
**  DKIM_BASE64_ENCODE -- encode base64 data
**
**  Parameters:
**  	data -- data to encode
**  	datalen -- bytes at "data" to encode
**  	buf -- where to write the encoding
**  	buflen -- bytes available at "buf"
**
**  Return value:
**  	>= 0 -- success; number of bytes written to "buf" returned
**   	-1 -- failure (not enough space at "buf")
*/

int
base64_encode(u_char *data, size_t datalen, u_char *buf, size_t buflen)
{
	int bits;
	int c;
	int char_count;
	size_t n;

	assert(data != NULL);
	assert(buf != NULL);

	bits = 0;
	char_count = 0;
	n = 0;

	for (c = 0; c < datalen; c++)
	{
		bits += data[c];
		char_count++;
		if (char_count == 3)
		{
			if (n + 4 > buflen)
				return -1;

			buf[n++] = alphabet[bits >> 18];
			buf[n++] = alphabet[(bits >> 12) & 0x3f];
			buf[n++] = alphabet[(bits >> 6) & 0x3f];
			buf[n++] = alphabet[bits & 0x3f];
			bits = 0;
			char_count = 0;
		}
		else
		{
			bits <<= 8;
		}
	}

	if (char_count != 0)
	{
		if (n + 4 > buflen)
			return -1;

		bits <<= 16 - (8 * char_count);
		buf[n++] = alphabet[bits >> 18];
		buf[n++] = alphabet[(bits >> 12) & 0x3f];
		if (char_count == 1)
		{
			buf[n++] = '=';
			buf[n++] = '=';
		}
		else
		{
			buf[n++] = alphabet[(bits >> 6) & 0x3f];
			buf[n++] = '=';
		}
	}

	return n;
}


/*
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
int main(int argc, char **argv)
{
	char *type = argv[1];
	char *str = NULL;
	char *out = NULL;
 
    if (type[0] == '-' && type[1] == 'e') {
		str = argv[2];
		out = (char *)calloc(1, strlen(str) * 4);
		if (out == NULL) {
			fprintf(stderr, "calloc fail:%s", strerror(errno));
			exit(1);
		}

		base64_encode(str, strlen(str), out, strlen(str) * 4);
		printf("base64_encode(\"%s\"): %s\n", str, out);
	} else {
		str = (char *)calloc(1, strlen(argv[2]) + 1);
		if (str == NULL) {
			printf("calloc memory fail\n");
			return -1;
		}
		memcpy(str, argv[2], strlen(argv[2]));
		str[strlen(argv[2])] = '\n';
		str[strlen(argv[2])+1] = '\0';
		//out = base64_decode(str, strlen(str));
		out = (char *)calloc(1, strlen(str));
		if (out == NULL) {
			fprintf(stderr, "calloc fail:%s", strerror(errno));
			exit(1);
		}

		int n = base64_decode(str, out, strlen(str));
	
		printf("base64_decode(\"%s\"): %s\n", argv[2], out);
		free(str);
	}
 
	free(out);
 
	return 0;
}
*/



int sj_b64_decode(MFILE *mfp_in, char *pre_buf, int pre_buf_size, FILE *fp)
{
	int b64_len = 0;
	int r = 0;
	int n = 0;
	int bits = 0;	
	int char_count = 0;	
	u_char *c;

	char buf[BUF_SIZE] = {0};
	memset(buf, 0, sizeof(buf));

	assert(pre_buf != NULL);	
	assert(buf != NULL);

	if (pre_buf_size > 0) {

		for (c = pre_buf; *c != '=' && *c != '\0'; c++) {
			/* end padding */
			if (*c == '=' || *c == '\0')
				break;
			
			/* skip stuff not part of the base64 alphabet (RFC2045) */
			if (!((*c >= 'A' && *c <= 'Z') ||
				  (*c >= 'a' && *c <= 'z') ||
				  (*c >= '0' && *c <= '9') ||
				  (*c >= 'a' && *c <= 'z') ||
				  (*c == '+') ||
				  (*c == '/')))
				continue;

			/* everything else gets decoded */
			bits += decoder[(int) *c];		
			char_count++;		
			if (n + 3 > BUF_SIZE)
				return -2;

			if (char_count == 4) {
				buf[n++] = (bits >> 16);			
				buf[n++] = ((bits >> 8) & 0xff);			
				buf[n++] = (bits & 0xff);			
				bits = 0;			
				char_count = 0;
			} else {
				bits <<= 6;
			}
	
		}	
	}	

	
	log_debug("write pre_buf[%d]:%s", pre_buf_size, pre_buf);
	log_debug("write pre_buf last 5:%c%c%c%c%c", pre_buf[pre_buf_size-5], pre_buf[pre_buf_size-4], pre_buf[pre_buf_size-3], pre_buf[pre_buf_size-2], pre_buf[pre_buf_size-1]);
	b64_len += n;
	r = fwrite(buf, n, 1, fp);
	if (r != 1) {
		return -1;
	}

	
	char str[BUF_SIZE] = {0};
	while (1) {
		memset(str, 0, sizeof(str));
		memset(buf, 0, sizeof(buf));
		n = 0;

		r = mread(mfp_in, str, (sizeof(str) / 2));
		if (r == 0)
			break;

		log_debug("write str[%d]:%s", r, str);
		log_debug("write str last 5:%c%c%c%c%c", str[r-5], str[r-4], str[r-3], str[r-2], str[r-1]);
		for (c = str; *c != '=' && *c != '\0'; c++) {
			/* end padding */
			if (*c == '=' || *c == '\0')
				break;

			/* skip stuff not part of the base64 alphabet (RFC2045) */
			if (!((*c >= 'A' && *c <= 'Z') ||
				  (*c >= 'a' && *c <= 'z') ||
				  (*c >= '0' && *c <= '9') ||
				  (*c >= 'a' && *c <= 'z') ||
				  (*c == '+') ||
				  (*c == '/')))
				continue;

			/* everything else gets decoded */
			bits += decoder[(int) *c];		
			char_count++;		
			if (n + 3 > sizeof(buf))
				return -2;

			if (char_count == 4) {
				buf[n++] = (bits >> 16);			
				buf[n++] = ((bits >> 8) & 0xff);			
				buf[n++] = (bits & 0xff);			
				bits = 0;			
				char_count = 0;
			} else {
				bits <<= 6;
			}
			
		}

		if (n > 0) {
			b64_len += n;
			log_debug("write buf[%d]:%d:%c buf[%d]:%d:%c", n-1, buf[n-1], buf[n-1], n, buf[n], buf[n]);
			r = fwrite(buf, n, 1, fp);
			if (r != 1) {
				return -1;
			}
		}

	}

	memset(buf, 0, sizeof(buf));
	n = 0;

	/* XXX -- don't bother checking for proper termination (for now) */

	/* process trailing data, if any */
	log_debug("char_count:%d", char_count);
	switch (char_count)
	{
		case 0:
			break;

		case 1:
			/* base64 decoding incomplete; at least two bits missing */
			return -1;

		case 2:
			buf[n++] = (bits >> 10);
			break;

		case 3:
			buf[n++] = (bits >> 16);
			buf[n++] = ((bits >> 8) & 0xff);
			break;
	}


	if (char_count == 2 || char_count == 3) {
		b64_len += n;
		log_debug("write final buf[%d]:%d:%c buf[%d]:%d:%c", n-1, buf[n-1], buf[n-1], n, buf[n], buf[n]);
		r = fwrite(buf, n, 1, fp);
		if (r != 1) {
			return -1;
		}
	}

	return b64_len;
}


