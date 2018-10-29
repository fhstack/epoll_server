all:epoll_server epoll_server_ET

epoll_server:epoll_server.c
	gcc -o $@ $^

epoll_server_ET:epoll_server_ET.c
	gcc -o $@ $^

.PHONY:
	clean
clean:
	rm -f epoll_server

