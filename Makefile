CC = cc
LIBS = -lX11 -lGL -lleif -lclipboard -lxcb -lxcb-randr -lm
SRC = boron.c
BIN = boron

all:
	mkdir -p ./bin
	${CC} -o ./bin/${BIN} ${SRC} ${LIBS}
install:
	cp bin/${BIN} /usr/bin
	cp scripts/* /usr/bin 
	mkdir -p /usr/share/boron
	mkdir -p /usr/share/boron/config
