#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include "str.h"
#include "smail_base64.h"

static char *b64alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#define B64PAD '='

/* returns 0 ok, 1 illegal, -1 problem */

int b64decode(const char *in, int l, stralloc *out)		/* not null terminated */
{
	int p = 0;
	int n;
	unsigned int x;
	int i, j;
	char *s;
	unsigned char b[3];

	if (l == 0)
	{
		if (!stralloc_copys(out, ""))
			return -1;
		return 0;
	}

	while (in[l - 1] == B64PAD)
	{
		p++;
		l--;
	}

	n = (l + p) / 4;
	i = (n * 3) - p;
	if (!stralloc_ready(out, i))
		return -1;
	out->len = i;
	s = out->s;
    memset(out->s, 0, out->a);

	for (i = 0; i < n - 1; i++)
	{
		x = 0;
		for (j = 0; j < 4; j++)
		{
			if (in[j] >= 'A' && in[j] <= 'Z')
				x = (x << 6) + (unsigned int) (in[j] - 'A' + 0);
			else if (in[j] >= 'a' && in[j] <= 'z')
				x = (x << 6) + (unsigned int) (in[j] - 'a' + 26);
			else if (in[j] >= '0' && in[j] <= '9')
				x = (x << 6) + (unsigned int) (in[j] - '0' + 52);
			else if (in[j] == '+')
				x = (x << 6) + 62;
			else if (in[j] == '/')
				x = (x << 6) + 63;
			else if (in[j] == '=')
				x = (x << 6);
		}

		s[2] = (unsigned char) (x & 255);
		x >>= 8;
		s[1] = (unsigned char) (x & 255);
		x >>= 8;
		s[0] = (unsigned char) (x & 255);
		x >>= 8;
		s += 3;
		in += 4;
	}

	x = 0;
	for (j = 0; j < 4; j++)
	{
		if (in[j] >= 'A' && in[j] <= 'Z')
			x = (x << 6) + (unsigned int) (in[j] - 'A' + 0);
		else if (in[j] >= 'a' && in[j] <= 'z')
			x = (x << 6) + (unsigned int) (in[j] - 'a' + 26);
		else if (in[j] >= '0' && in[j] <= '9')
			x = (x << 6) + (unsigned int) (in[j] - '0' + 52);
		else if (in[j] == '+')
			x = (x << 6) + 62;
		else if (in[j] == '/')
			x = (x << 6) + 63;
		else if (in[j] == '=')
			x = (x << 6);
	}

	b[2] = (unsigned char) (x & 255);
	x >>= 8;
	b[1] = (unsigned char) (x & 255);
	x >>= 8;
	b[0] = (unsigned char) (x & 255);
	x >>= 8;

	for (i = 0; i < 3 - p; i++)
		s[i] = b[i];

	return 0;
}

int b64encode(stralloc *in, stralloc *out)		/* not null terminated */
{
	unsigned char a, b, c;
	int i;
	char *s;

	if (in->len == 0)
	{
		if (!stralloc_copys(out, ""))
			return -1;
		return 0;
	}

	i = in->len / 3 * 4 + 4;
	if (!stralloc_ready(out, i + 1))
		return -1;
	s = out->s;
    memset(out->s, 0, out->a);

	for (i = 0; i < in->len; i += 3)
	{
		a = in->s[i];
		b = i + 1 < in->len ? in->s[i + 1] : 0;
		c = i + 2 < in->len ? in->s[i + 2] : 0;

		*s++ = b64alpha[a >> 2];
		*s++ = b64alpha[((a & 3) << 4) | (b >> 4)];

		if (i + 1 >= in->len)
			*s++ = B64PAD;
		else
			*s++ = b64alpha[((b & 15) << 2) | (c >> 6)];

		if (i + 2 >= in->len)
			*s++ = B64PAD;
		else
			*s++ = b64alpha[c & 63];
	}
	out->len = s - out->s;
	return 0;
}


#ifdef TEST_B64
#include <stdio.h>
int main(int argc, char *argv[])
{
    int iret = 0;
    char *src="MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1Kd87/UeJjenpabgbFwh+eBCsSTrqmwIYYvywlbhbqoo2DymndFkbjOVIPIldNs/m40KF+yzMn1skyoxcTUGCQs8g3FgD2Ap3ZB5DekAo5wMmk4wimDO+U8QzI3SD07y2+07wlNWwIt8svnxgdxGkVbbhzY8i+RQ9DpSVpPbF7ykQxtKXkv/ahW3KjViiAH+ghvvIhkx4xYSIc9oSwVmAl5OctMEeWUwg8Istjqz8BZeTWbf41fbNhte7Y+YqZOwq1Sd0DbvYAD9NOZK9vlfuac0598HY+vtSBczUiKERHv1yRbcaQtZFh5wtiRrN04BLUTD21MycBX5jYchHjPY/wIDAQAB";
    stralloc out = {0};

    iret = b64decode(src, strlen(src), &out);

    printf("iret:%d, dest len:%d\n", iret, out.len);

    return 0;
}

#endif


