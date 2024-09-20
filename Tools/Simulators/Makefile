OS := $(shell uname)
LIBS :=
INCLUDES :=
ifeq (${OS},Darwin)
UNAME_V := $(shell uname -v)
ifeq ($(UNAME_V),*ARM64*)
# Detect Mac Apple Silicon
INCLUDES := -I/usr/local/include/
LIBS := -L/usr/local/Cellar/popt/1.18/lib/
else
# Detect Mac Intel
INCLUDES := -I/opt/homebrew/include/
LIBS := -L/opt/homebrew/Cellar/popt/1.19/lib/
endif
endif
# Detect Windows/MinGW. Use -static in order for .exe file to be easily shareable
ifneq (,$(findstring MINGW,${OS}))
LIBS := -lintl -liconv -static
endif

CFLAGS := -g -O

ESP_DIR := ../../ESP

all: faikin-x50 faikin-s21 s21-control

osal.o : osal.c osal.h
	gcc $(CFLAGS) -c -o $@ $<

s21_state_parser.o : s21_state_parser.c faikin-s21.h
	gcc $(CFLAGS) -c -o $@ $<

faikin-s21.o : faikin-s21.c faikin-s21.h osal.h ${ESP_DIR}/main/daikin_s21.h ${ESP_DIR}/main/faikin_enums.h
	gcc $(CFLAGS) -c -o $@ $< -I${ESP_DIR} ${INCLUDES}

s21-control.o : s21-control.c osal.h
	gcc $(CFLAGS) -c -o $@ $<

faikin-x50.o : faikin-x50.c osal.h
	gcc $(CFLAGS) -c -o $@ $< -I${ESP_DIR} ${INCLUDES}

faikin-x50: faikin-x50.o osal.o
	gcc -o $@ $^ -lpopt ${LIBS}

faikin-s21: faikin-s21.o s21_state_parser.o osal.o
	gcc -o $@ $^ ${LIBS}

s21-control: s21-control.o s21_state_parser.o osal.o
	gcc -o $@ $^ ${LIBS}

clean:
	rm -f faikin-x50 faikin-s21 s21-control faikin-x50.exe faikin-s21.exe s21-control.exe *.o
