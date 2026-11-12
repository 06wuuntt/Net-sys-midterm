CC     = gcc
CFLAGS = -Wall -Wextra -g -fPIC
B      = build

all: $(B)/libutil.so $(B)/server $(B)/client

$(B):
	mkdir -p $(B)

$(B)/libutil.so: src/util.c src/util.h | $(B)
	$(CC) $(CFLAGS) -shared -o $@ src/util.c

$(B)/server: src/server.c src/util.h $(B)/libutil.so
	$(CC) $(CFLAGS) -o $@ src/server.c -L$(B) -lutil -Wl,-rpath,'$$ORIGIN'

$(B)/client: src/client.c src/util.h $(B)/libutil.so
	$(CC) $(CFLAGS) -o $@ src/client.c -L$(B) -lutil -Wl,-rpath,'$$ORIGIN'

# concurrency proof: 12 clients at once, each holds a connection for 5 s
test: all
	./scripts/test_concurrent.sh

clean:
	rm -rf $(B)
.PHONY: all test clean
