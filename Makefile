# Root Makefile for Monopoly Network Game
# Builds both server and client

.PHONY: all server client clean run-server run-client

all: server client

server:
	@echo "Building server..."
	$(MAKE) -C src/server

client:
	@echo "Building client..."
	$(MAKE) -C src/client

clean:
	@echo "Cleaning build..."
	rm -rf build/
	$(MAKE) -C src/server clean
	$(MAKE) -C src/client clean

run-server: server
	./build/server/monopoly_server -p 8888 -d monopoly.db

run-client: client
	SDL_VIDEODRIVER=x11 ./build/client/monopoly_client 127.0.0.1 8888

# Development helpers
rebuild: clean all

test-client:
	$(MAKE) -C src/client test
	./build/client/monopoly_client_test 127.0.0.1 8888
