CC			= gcc
BIN			= boron
DEPLIBS = -lrunara -lfreetype -lharfbuzz -lfontconfig -lm -lGL -lglfw -lX11 -lXinerama -lXrandr -lXrender -lragnar -lpodvig -lasound -lpthread
CFLAGS	= -DLF_X11 -DLF_RUNARA -ffast-math -O3 -pedantic  -g
LIBS 		=-L../lib -lleif  ${DEPLIBS} 

all:
	${CC} -o bin/${BIN}  ${CFLAGS}  src/*.c ${LIBS}

install:
	cp ./bin/boron /usr/bin
	cp ./scripts/* /usr/bin
