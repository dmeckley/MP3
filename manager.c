// File: manager.c
// Programmer: Dustin Meckley
// Date: 12/10/2015

#include "simulator.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int create_listening_socket()
{
	int listening_sock_fd;
	struct sockaddr_in server_addr;

	/* create socket */
	if (-1 == (listening_sock_fd = socket(AF_INET, SOCK_STREAM, 0)))
	{
		debug_printf("Error! Cannot create listening socket for manager %d. Reason: %s.\n", getpid(), strerror(errno));
		exit(1);
	}
	debug_printf("Manager %d created listening socket %d.\n", getpid(), listening_sock_fd);
	
	/* initialize the socket fields */
	memset((char *) &server_addr, 0, sizeof (server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 
	server_addr.sin_port = htons(MANAGER_PORT_NO);

	if (-1 == bind(listening_sock_fd, (struct sockaddr *) &server_addr, sizeof (server_addr)))
	{
		debug_printf("Manager %d could not bind socket %d. Reason: %s.\n", getpid(), listening_sock_fd, strerror(errno));
		exit(1);
	}
	debug_printf("Manager %d bound listening socket.\n", getpid());

	if (-1 == listen(listening_sock_fd, 1024))
	{
		debug_printf("Manager %d could not listen to listening socket %d. Reason: %s.\n", getpid(), listening_sock_fd, strerror(errno));
		exit(1);
	}
	debug_printf("Manager %d listens to listening socket.\n", getpid());

	return listening_sock_fd;
}

void run_manager(int nrouters, int **neighbors, FILE *finPackets)
{
	int i, j;
	char buffer[BUFFER_SIZE];

	int listening_sock_fd = create_listening_socket();
	int *client_sock_fds = (int*) calloc(nrouters, sizeof (int));
	int *client_udp_port_nos = (int*) calloc(nrouters, sizeof (int));

	/* Manager receives CONFIG from each router about its ID and UDP port no */
	for (i = 0; i < nrouters; ++i)
	{
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof (client_addr);

		if (-1 == (client_sock_fds[i] = accept(listening_sock_fd, (struct sockaddr *) &client_addr, (socklen_t*) & client_len)))
		{
			debug_printf("Manager %d couldn't accept a client connection at socket %d. Reason: %s\n", getpid(), listening_sock_fd, strerror(errno));
			exit(1);
		}

		read_with_timeout(client_sock_fds[i], buffer, DEFAULT_TIMEOUT);
		sscanf(buffer, "CONFIG %d", &client_udp_port_nos[i]);
		debug_printf("Manager %d received '%s' from socket %d.\n", getpid(), buffer, client_sock_fds[i]);
	}

	close(listening_sock_fd);
	debug_printf("Manager %d closed listening socket.\n", getpid());

	/* Manager replies with SETUP to each router */
	for (i = 0; i < nrouters; ++i)
	{
		sprintf(buffer, "SETUP %d %d", i, nrouters);
		for (j = 0; j < nrouters; ++j)
		{
			char buf[10];
			
			/* each router is included in the SETUP response, including self */
			sprintf(buf, " %d %d", client_udp_port_nos[j], neighbors[i][j]);
			strcat(buffer, buf);
		}
		write_with_timeout(client_sock_fds[i], buffer, DEFAULT_TIMEOUT);
		debug_printf("Manager %d replied with '%s' to socket %d.\n", getpid(), buffer, client_sock_fds[i]);
	}
	
	/* UDP port numbers of Routers are no longer needed */
	free(client_udp_port_nos);

	/* Manager receives READY from each router and replies with BEGIN */
	for (i = 0; i < nrouters; ++i)
	{
		read_with_timeout(client_sock_fds[i], buffer, DEFAULT_TIMEOUT);
		if (0 != strncmp(buffer, "READY", 5))
		{
			debug_printf("Error. Manager %d received '%s' from Router %d, but expected READY.\n", getpid(), buffer, i);
			exit(1);
		}
		else
			debug_printf("Manager %d received '%s' from socket %d.\n", getpid(), buffer, client_sock_fds[i]);

		write_with_timeout(client_sock_fds[i], "BEGIN", DEFAULT_TIMEOUT);
		debug_printf("Manager %d replied with '%s' to socket %d.\n", getpid(), "BEGIN", client_sock_fds[i]);
	}

	/* Manager sends PRINT to each router */
	for (i = 0; i < nrouters; ++i)
	{
		write_with_timeout(client_sock_fds[i], "PRINT", DEFAULT_TIMEOUT);
		debug_printf("Manager %d sent '%s' to socket %d.\n", getpid(), "PRINT", client_sock_fds[i]);
	}

	/* Manager receives DONE from each router */
	for (i = 0; i < nrouters; ++i)
	{
		read_with_timeout(client_sock_fds[i], buffer, DEFAULT_TIMEOUT);
		if (0 != strcmp(buffer, "DONE"))
		{
			debug_printf("Error. Manager %d received '%s' from Router %d, but expected DONE.\n", getpid(), buffer, i);
			exit(1);
		}
		else
			debug_printf("Manager %d received '%s' from socket %d.\n", getpid(), buffer, client_sock_fds[i]);
	}

	flushed_printf("\n");
	
	/* Manager reads packets and sends SOURCE messages to routers */
	if (finPackets)
	{
		int from, to;
		int packet_no = 0;

		while (2 == fscanf(finPackets, "%d %d", &from, &to))
		{
			flushed_printf("Trace #%d\n", ++packet_no);
			usleep(1000 * DEFAULT_TIMEOUT / 4); /* wait 1 second */
			
			sprintf(buffer, "SOURCE dest=%d packet=%d", to, packet_no);
			
			write_with_timeout(client_sock_fds[from], buffer, DEFAULT_TIMEOUT);
			
			read_with_timeout(client_sock_fds[to], buffer, DEFAULT_TIMEOUT);
			if (0 != strncmp(buffer, "DESTINATION", 11))
				debug_printf("Error! Manager %d received '%s' from socket %d but expected: 'DESTINATION'.\n", getpid(), buffer, client_sock_fds[to]);
			
			usleep(1000 * SLICE_TIMEOUT); /* wait 100 milliseconds */
			flushed_printf("\n");
		}
	}

	usleep(1000 * SLICE_TIMEOUT); /* wait 100 milliseconds */

	/* Manager sends CLOSE message to each router and closes the connections */
	for (i = 0; i < nrouters; ++i)
	{
		write_with_timeout(client_sock_fds[i], "CLOSE", DEFAULT_TIMEOUT);
		close(client_sock_fds[i]);
		debug_printf("Manager %d sent '%s' to socket %d and closed it.\n", getpid(), "CLOSE", client_sock_fds[i]);
	}
	free(client_sock_fds);
	
	usleep(1000 * SLICE_TIMEOUT); /* wait 100 milliseconds */
}


