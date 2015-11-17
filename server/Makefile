CC = gcc -g
INCS = -I./confparser/ -I./libmfile/ -I/usr/include/ -I/usr/local/include/ -I/usr/local/include/ImageMagick-6/ -I/usr/local/mysql/include/
LDS = -L/usr/local/lib64/ -L/usr/local/lib/ -L/usr/local/mysql/lib/ -lMagickWand-6.Q16 -lmysqlclient -lcurl -lmemcached -lz -lssl

OBJS = liao_server liao_command

default: liao_server liao_command

liao_server:	confparser/libconfparser.a \
			liao_server.o liao_log.o	\
			base64.o \
			libmfile/libmfile.a
	$(CC) -o liao_server liao_server.o \
			liao_log.o base64.o  \
			confparser/libconfparser.a \
			libmfile/libmfile.a $(INCS) $(LDS)

liao_command:	liao_command.o \
			confparser/libconfparser.a \
			liao_log.o  \
			liao_utils.o    \
			base64.o    \
			libmfile/libmfile.a
	$(CC) -o liao_command liao_command.o \
			 liao_log.o liao_utils.o \
			base64.o \
			confparser/libconfparser.a \
			libmfile/libmfile.a $(INCS) $(LDS)

liao_server.o:	liao_server.c
	$(CC) -c -o liao_server.o liao_server.c $(INCS)

liao_command.o: liao_command.c
	$(CC) -c -o liao_command.o liao_command.c $(INCS)

liao_log.o:	liao_log.c
	$(CC) -c -o liao_log.o liao_log.c $(INCS)

liao_utils.o: liao_utils.c
	$(CC) -c -o liao_utils.o liao_utils.c $(INCS) -DMAGICKCORE_QUANTUM_DEPTH=16 -DMAGICKCORE_HDRI_ENABLE=1

liao_commands.o: liao_commands.c
	$(CC) -c -o liao_commands.o liao_commands.c $(INCS)

base64.o: base64.c
	$(CC) -c -o base64.o base64.c $(INCS)

error_temp.o: error_temp.c
	$(CC) -c -o error_temp.o error_temp.c $(INCS)

error.o: error.c
	$(CC) -c -o error.o error.c $(INCS)

confparser/libconfparser.a:
	cd confparser; make -f linux.mk; cd -

libmfile/libmfile.a:
	cd libmfile; make; cd -




clean:
	cd confparser; make -f linux.mk clean; cd -
	cd libmfile; make clean; cd -
	rm -rf *.o
	rm -rf $(OBJS)
	
