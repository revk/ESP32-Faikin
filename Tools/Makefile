# Tools makefile (linux/Debian)

ifeq ($(wildcard /bin/csh),)
$(error	Please install /bin/csh or equivalent)
endif

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

ifdef	SQLINC
TOOLS := faikin faikinlog faikingraph
else
$(warning Warning - mariadb/mysql not installed, needed if you want to build tools)
endif

tools:	$(TOOLS)

ifeq ($(shell uname),Darwin)
INCLUDES=-I/usr/local/include/ -I../ESP32/main
LIBS=-L/usr/local/Cellar/popt/1.18/lib/
else
LIBS=
INCLUDES=-I../ESP32/main
endif

SQLlib/sqllib.o: SQLlib/sqllib.c
	make -C SQLlib
AXL/axl.o: AXL/axl.c
	make -C AXL
AJL/ajl.o: AJL/ajl.c
	make -C AJL

CCOPTS=${SQLINC} -I. -I/usr/local/ssl/include -D_GNU_SOURCE -g -Wall -funsigned-char -lm
OPTS=-L/usr/local/ssl/lib ${SQLLIB} ${CCOPTS}

faikin: faikin.c
	gcc -O -o $@ $< -lpopt ${INCLUDES} ${LIBS}

faikin-s21: faikin-s21.c ../ESP32/main/daikin_s21.h ../ESP32/main/faikin_enums.h
	gcc -O0 -g -o $@ $< -lpopt ${INCLUDES} ${LIBS}

faikinlog: faikinlog.c SQLlib/sqllib.o AJL/ajl.o ../ESP32/main/acextras.m ../ESP32/main/acfields.m ../ESP32/main/accontrols.m
	cc -O -o $@ $< -lpopt -lmosquitto -ISQLlib SQLlib/sqllib.o -IAJL AJL/ajl.o ${INCLUDES} ${OPTS}

faikingraph: faikingraph.c SQLlib/sqllib.o AXL/axl.o
	cc -O -o $@ $< -lpopt -lmosquitto -ISQLlib SQLlib/sqllib.o -IAXL AXL/axl.o -lcurl ${INCLUDES} ${OPTS}
pull:
	git pull
	git submodule update --recursive

update:
	git submodule update --init --recursive --remote
	-git commit -a -m "Library update"
