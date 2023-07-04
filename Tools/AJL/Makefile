all: ajl ajl.o ajlparse.o ajlcurl.o jcgi.o jcgi

ifeq ($(shell uname),Darwin)
INCLUDES=-I/usr/local/include/
LIBS=-L/usr/local/Cellar/popt/1.18/lib/
else
LIBS=
INCLUDES=
endif

ajlparse.o: ajlparse.c ajlparse.h Makefile
	gcc -g -Wall -Wextra -DLIB -O -c -o ajlparse.o ajlparse.c -std=gnu99 -D_GNU_SOURCE ${INCLUDES}

ajl.o: ajlparse.c ajlparse.h ajl.c ajl.h Makefile
	gcc -g -Wall -Wextra -DLIB -O -c -o ajl.o ajl.c -std=gnu99 -D_GNU_SOURCE ${INCLUDES}

ajlcurl.o: ajlparse.c ajlparse.h ajl.c ajl.h ajlcurl.h Makefile
	gcc -g -Wall -Wextra -DLIB -O -c -o ajlcurl.o ajl.c -std=gnu99 -D_GNU_SOURCE -lcurl -DJCURL ${INCLUDES}

ajl: ajlparse.c ajlparse.h ajl.c ajl.h Makefile
	gcc -g -Wall -Wextra -O -o ajl ajl.c -std=gnu99 -D_GNU_SOURCE -lcurl -DJCURL -lpopt ${INCLUDES} ${LIBS}

jcgi.o: jcgi.c ajl.h jcgi.h ajlparse.h Makefile
	gcc -g -Wall -Wextra -DLIB -O -c -o jcgi.o jcgi.c -std=gnu99 -D_GNU_SOURCE ${INCLUDES}

jcgi: jcgi.c ajl.o jcgi.h Makefile
	gcc -g -Wall -Wextra -O -o jcgi jcgi.c ajl.o -std=gnu99 -D_GNU_SOURCE -lpopt ${INCLUDES} ${LIBS}

clean:
	rm -f *.o ajl
