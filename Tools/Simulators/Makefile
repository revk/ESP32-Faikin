ifeq ($(shell uname),Darwin)
INCLUDES := -I/usr/local/include/
LIBS := -L/usr/local/Cellar/popt/1.18/lib/
else
LIBS :=
INCLUDES :=
endif

ESP_DIR := ../../ESP

faikin-x50: faikin-x50.c
	gcc -O -o $@ $< -lpopt -I${ESP_DIR} ${INCLUDES} ${LIBS}

faikin-s21: faikin-s21.c ${ESP_DIR}/main/daikin_s21.h ${ESP_DIR}/main/faikin_enums.h
	gcc -O -g -o $@ $< -lpopt -I${ESP_DIR} ${INCLUDES} ${LIBS}

all: faikin-x50 faikin-s21
