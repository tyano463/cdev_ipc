

CC		:= gcc
CFLAGS 	:= -g -O0
#CFLAGS 	:= -O3

OBJS_A	:= tx.o
OBJS_B	:= rx.o
EXE_A		:= tx
EXE_B		:= rx

all:$(EXE_A) $(EXE_B)
	make -C driver

$(EXE_A): $(OBJS_A)
	$(CC) $(LDFLAGS) $^ -o $@ 

$(EXE_B): $(OBJS_B)
	$(CC) $(LDFLAGS) $^ -o $@ 

.c.o:
	$(CC) $(CFLAGS) -c $<


clean:
	rm -f $(OBJS_A) $(OBJS_B) $(EXE_A) $(EXE_B)
	make -C driver clean
