all: httpd simpleclient

httpd: httpd.c
	gcc -W -Wall -lpthread -o httpd httpd.c

simpleclient: simpleclient.c
	gcc -W -Wall -o simpleclient simpleclient.c

clean:
	rm httpd simpleclient
