CC = cc
LIBS = -lX11 -lGL -lleif -lclipboard -lxcb -lxcb-randr -lm
SRC = boron.c
BIN = boron

all:
	${CC} -o ${BIN} ${SRC} ${LIBS}
install:
	cp boron /usr/bin
