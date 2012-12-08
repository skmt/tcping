#
# tcping's Makefile
#

#CC	= cc
CC	= gcc
TAGS	= ctags	# for vi
#COMP 	= compress
COMP 	= gzip
DEBUG	= # -DDEBUG
DATE	= `date +%Y%m%d`
OPTIM	= -O
#CFLAGS	= -pg ${OPTIM} ${DEBUG}
CFLAGS	= ${OSTYPE} -g -Wall ${OPTIM} ${DEBUG}
LDFLAGS	= # -static
LIBS	= 
INCS	= 
OBJS	= tcping.o
SRCS	= tcping.c

TARGET	= tcping


all:${TARGET}

ctags:
	ctags *.c *.h

etags:
	etags *.c *.h

${TARGET}:${OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^ ${LIBS}

touch:
	touch *.c

.c.o:

.h.c:


clean:
	rm -f core *.exe.stackdump *.o *.exe ${TARGET} gmon.out mtrace.out

tar:
	tar cvf - ${SRCS} ${INCS} Makefile | ${COMP} - > ${TARGET}.tgz
	[ ! -d ./Backup ] && mkdir Backup
	-mv ${TARGET}.tgz Backup/${TARGET}.tgz.${DATE}

#
# test suite
#
test:
	@/bin/echo " --- start tcping test ==>"
	@./tcping -h
	@/bin/echo "successfully done --- "

# end of makefile
