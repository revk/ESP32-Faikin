ifneq ($(wildcard /usr/bin/mysql_config),)
	SQLINC=$(shell mysql_config --include)
	SQLLIB=$(shell mysql_config --libs)
	SQLVER=$(shell mysql_config --version | sed 'sx\..*xx')
endif
ifneq ($(wildcard /usr/bin/mariadb_config),)
	SQLINC=$(shell mariadb_config --include)
	SQLLIB=$(shell mariadb_config --libs)
	SQLVER=$(shell mariadb_config --version | sed 'sx\..*xx')
endif
OPTS=-D_GNU_SOURCE --std=gnu99 -g -Wall -funsigned-char -lpopt

all: sqllib.o sqllibsd.o sqlexpand.o sql sqlwrite sqledit sqlexpand

update:
	git pull
	git submodule update --init --remote --recursive
	make -C stringdecimal

stringdecimal/stringdecimal.o:
	git submodule update --recursive
	make -C stringdecimal

sqllib.o: sqllib.c sqllib.h Makefile
	gcc -g -O -c -o $@ $< -fPIC ${OPTS} -DLIB ${SQLINC} -DMYSQL_VERSION=${SQLVER}

sqllibsd.o: sqllib.c sqllib.h Makefile stringdecimal/stringdecimal.o
	gcc -g -O -c -o $@ $< -fPIC ${OPTS} -DLIB ${SQLINC} -DMYSQL_VERSION=${SQLVER} -DSTRINGDECIMAL

sql: sql.c sqllibsd.o sqllib.h sqlexpand.o sqlexpand.h stringdecimal/stringdecimal.o
	gcc -g -O -o $@ $< -fPIC ${OPTS} -DNOXML ${SQLINC} ${SQLLIB} sqllibsd.o sqlexpand.o stringdecimal/stringdecimal.o -lcrypto -luuid

sqlwrite: sqlwrite.c sqllibsd.o sqllib.h stringdecimal/stringdecimal.o
	gcc -g -O -o $@ $< -fPIC ${OPTS} ${SQLINC} ${SQLLIB} sqllibsd.o stringdecimal/stringdecimal.o

sqledit: sqledit.c sqllibsd.o sqllib.h stringdecimal/stringdecimal.o
	gcc -g -O -o $@ $< -fPIC ${OPTS} ${SQLINC} ${SQLLIB} sqllibsd.o stringdecimal/stringdecimal.o

sqlexpand.o: sqlexpand.c Makefile
	cc -c -o $@ $< ${OPTS} -DLIB

sqlexpand: sqlexpand.c Makefile
	cc -O -o $@ $< ${OPTS} -luuid -lcrypto
