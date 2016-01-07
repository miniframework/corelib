WORKROOT=../
CC=g++
LIBPATH=$(WORKROOT)lib

INCLUDES= -I$(WORKROOT)include
CFLAGS = -fsigned-char -Wall -W -pipe -Wno-unused-parameter -g -D__XDEBUG__ -D__YDEBUG__

LDFLAGS=  -L$(LIBPATH) \
  -lminicore -lpthread -lm
OBJS=test_open_log.o
TARGET=test_open_log

all : $(TARGET) $(LIB)
	rm -f $(OBJS) 

%.o : %.cpp
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDES)


test_open_log: test_open_log.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(INCLUDES)

$(LIB) : $(OBJS)
	rm -f $@
	ar cr $@ $(OBJS)

clean:
	rm -f $(OBJS) $(TARGET)
