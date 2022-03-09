CFLAGS = -I./include

PROGS = allsyms symval size

all: $(PROGS)

readint.o: readint.c

allsyms: allsyms.c readint.c size.h proto.h include/osbind.h
	$(CC) $(CFLAGS) $< -o $@

symval: symval.c readint.c size.h proto.h include/osbind.h
	$(CC) $(CFLAGS) $< -o $@

size: size.c readint.o size.h proto.h include/osbind.h
	$(CC) $(CFLAGS) readint.o $< -o $@

.PHONY:clean
clean:
	rm -f $(PROGS) *.o
