
# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	SHOBJ_CFLAGS ?= -W -Wall -fno-common -g -ggdb -std=c99 -O2
	SHOBJ_LDFLAGS ?= -shared
else
	SHOBJ_CFLAGS ?= -W -Wall -dynamic -fno-common -g -ggdb -std=c99 -O2
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup
endif

.SUFFIXES: .c .so .xo .o

all: helloworld.so hellotype.so helloblock.so testmodule.so bloomfilter.so

.c.xo:
	$(CC) -I. $(CFLAGS) $(SHOBJ_CFLAGS) -fPIC -c $< -o $@
 
helloworld.xo: ../redismodule.h

helloworld.so: helloworld.xo
	$(LD) -o $@ $< $(SHOBJ_LDFLAGS) $(LIBS) -lc

bloomfilter.xo: bloomfilter.c ../redismodule.h
	$(CC) -I. $(CFLAGS) $(SHOBJ_CFLAGS)  `pkg-config --cflags glib-2.0` -fPIC -c $< -o $@

bloomfilter.so: bloomfilter.xo
	$(LD) -o $@ $< $(SHOBJ_LDFLAGS) $(LIBS) `pkg-config --libs glib-2.0` -lc ../../deps/dablooms/build/libdablooms.a
	
hellotype.xo: ../redismodule.h

hellotype.so: hellotype.xo
	$(LD) -o $@ $< $(SHOBJ_LDFLAGS) $(LIBS) -lc

helloblock.xo: ../redismodule.h

helloblock.so: helloblock.xo
	$(LD) -o $@ $< $(SHOBJ_LDFLAGS) $(LIBS) -lpthread -lc

testmodule.xo: ../redismodule.h

testmodule.so: testmodule.xo
	$(LD) -o $@ $< $(SHOBJ_LDFLAGS) $(LIBS) -lc

clean:
	rm -rf *.xo *.so
