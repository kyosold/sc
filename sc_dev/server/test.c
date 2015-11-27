#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "base64.h"

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

