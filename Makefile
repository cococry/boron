CC			= gcc
BIN			= boron
DEPLIBS = -lrunara -lfreetype -lharfbuzz -lfontconfig -lm -lGL -lglfw -lX11 -lXinerama
CFLAGS	= -DLF_X11 -DLF_RUNARA -ffast-math -O3 -pedantic  -g
LIBS 		=-L../lib -lleif  ${DEPLIBS} 

all:
	${CC} -o bin/${BIN}  ${CFLAGS}  *.c ${LIBS}

install:
	cp ./bin/boron /usr/bin 
