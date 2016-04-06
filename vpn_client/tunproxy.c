/*
 * tunproxy.c --- small demo program for tunneling over UDP with tun/tap
 *
 * Copyright (C) 2003  Philippe Biondi <phil@secdev.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */



#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <getopt.h>
#include <sys/ioctl.h>

#define PERROR(x) do { perror(x); exit(1); } while (0)
#define ERROR(x, args ...) do { fprintf(stderr,"ERROR:" x, ## args); exit(1); } while (0)

char MAGIC_WORD[] = "Wazaaaaaaaaaaahhhh !";


void usage()
{
	fprintf(stderr, "Usage: tunproxy [-s port|-c targetip:port] [-e]\n");
	exit(0);
}

int tun_alloc(char *dev, int flags) {

	struct ifreq ifr;
	int fd, err;
	char *clonedev = "/dev/net/tun";

	/* Arguments taken by the function:
	 *
	 * char *dev: the name of an interface (or '\0'). MUST have enough
	 *   space to hold the interface name if '\0' is passed
	 * int flags: interface flags (eg, IFF_TUN etc.)
	 */

	/* open the clone device */
	if( (fd = open(clonedev, O_RDWR)) < 0 ) {
		return fd;
	}

	/* preparation of the struct ifr, of type "struct ifreq" */
	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_flags = flags;   /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

	if (*dev) {
		/* if a device name was specified, put it in the structure; otherwise,
		 * the kernel will try to allocate the "next" device of the
		 * specified type */
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}

	/* try to create the device */
	if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
		close(fd);
		return err;
	}

	/* if the operation was successful, write back the name of the
	 * interface to the variable "dev", so the caller can know
	 * it. Note that the caller MUST reserve space in *dev (see calling
	 * code below) */
	strcpy(dev, ifr.ifr_name);

	/* this is the special file descriptor that the caller will use to talk
	 * with the virtual interface */
	return fd;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in sin, sout, from;
	struct ifreq ifr;
	int fd, s, fromlen, soutlen, port, PORT, l;
	char c, *p, *ip;
	char buf[1500];
	fd_set fdset;
	
	

	int MODE = 0, TUNMODE = IFF_TUN, DEBUG = 0;

	while ((c = getopt(argc, argv, "s:c:ehd")) != -1) {
		switch (c) {
		case 'h':
			usage();
		case 'd':
			DEBUG++;
			break;
		case 's':
			MODE = 1;
			// server;
			PORT = atoi("55555");
			break;
		case 'c':
			MODE = 2;
			// client;
			p = memchr(optarg,':',16);
			if (!p) ERROR("invalid argument : [%s]\n",optarg);
			*p = 0;
			ip = optarg;
			port = atoi(p+1);
			PORT = 0;
			break;
		case 'e':
			TUNMODE = IFF_TAP;
			break;
		default:
			usage();
		}
	}
	if (MODE == 0) usage();
//	char tun[100] = {0}; 
//	if (fd = tun_alloc(tun, IFF_TUN | IFF_NO_PI) < 0){
//		printf("tun alloc fail = fd");	
//	}

	if ( (fd = open("/dev/net/tun",O_RDWR)) < 0) PERROR("open");

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = TUNMODE;
	strncpy(ifr.ifr_name, "toto%d", IFNAMSIZ);
	if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) PERROR("ioctl");

	printf("Allocated interface %s. Configure and use it\n", ifr.ifr_name);
	
	s = socket(PF_INET, SOCK_DGRAM, 0);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(PORT);
	if ( bind(s,(struct sockaddr *)&sin, sizeof(sin)) < 0) PERROR("bind");

	fromlen = sizeof(from);

	if (MODE == 1) {
		while(1) {
			l = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
			if (l < 0) PERROR("recvfrom");
			if (strncmp(MAGIC_WORD, buf, sizeof(MAGIC_WORD)) == 0)
				break;
			printf("Bad magic word from %s:%i\n", 
			       inet_ntoa(from.sin_addr.s_addr), ntohs(from.sin_port));
		} 
		l = sendto(s, MAGIC_WORD, sizeof(MAGIC_WORD), 0, (struct sockaddr *)&from, fromlen);
		if (l < 0) PERROR("sendto");
	} else {
		printf("client \n\n");
		from.sin_family = AF_INET;
		from.sin_port = htons(port);
		inet_aton(ip, &from.sin_addr);
		l =sendto(s, MAGIC_WORD, sizeof(MAGIC_WORD), 0, (struct sockaddr *)&from, sizeof(from));
		if (l < 0) printf("sendto\n");
		l = recvfrom(s,buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
		if (l < 0) PERROR("recvfrom");
		if (strncmp(MAGIC_WORD, buf, sizeof(MAGIC_WORD) != 0))
			ERROR("Bad magic word for peer\n");
	}
	printf("Connection with %s:%i established\n", 
	       inet_ntoa(from.sin_addr.s_addr), ntohs(from.sin_port));
	int max_fd = fd > s ? fd:s;
	while (1) {
		FD_ZERO(&fdset);
		FD_SET(fd, &fdset);
		FD_SET(s, &fdset);
		if (select(max_fd + 1, &fdset,NULL,NULL,NULL) < 0) printf("select");
		if (FD_ISSET(fd, &fdset)) {
                        printf("tun is alive\n");
			if (DEBUG) write(1,">", 1);
			l = read(fd, buf, sizeof(buf));
			printf("read %d from tap\n",l);
			if (l < 0) PERROR("read");
			if (sendto(s, buf, l, 0, (struct sockaddr *)&from, fromlen) < 0) printf("sendto");
		} else {
			if (DEBUG) write(1,"<", 1);
			printf("net_fd is alive\n");
			l = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&sout, &soutlen);
			if ((sout.sin_addr.s_addr != from.sin_addr.s_addr) || (sout.sin_port != from.sin_port))
				printf("Got packet from  %s:%i instead of %s:%i\n", 
				       inet_ntoa(sout.sin_addr.s_addr), ntohs(sout.sin_port),
				       inet_ntoa(from.sin_addr.s_addr), ntohs(from.sin_port));
			if (write(fd, buf, l) < 0) PERROR("write");
		}
	}
}
