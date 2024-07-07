CC = cc
LIBS = -lX11 -lXrandr -lGL -lleif -lclipboard -lxcb -lm
SRC = boron.c
BIN = boron

all:
	${CC} -o ${BIN} ${SRC} ${LIBS}
