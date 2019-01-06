// File: router.c
// Programmer: Dustin Meckley
// Date: 12/10/2015

#include "simulator.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

struct router
{
	int udp_sock_fd;
	int direct_cost; 
	int best_cost;
	int next_hop_id;
	bool_t lsp_received;
};

void create_local_udp_socket(int *udp_sock_fd, int *udp_port_no)
{
	struct sockaddr_in udp_my_addr;
	socklen_t udp_sock_len = sizeof udp_my_addr;

	/* create the UDP socket */
	if (-1 == (*udp_sock_fd = socket(AF_INET, SOCK_DGRAM, 0)))
	{
		debug_printf("Error! Router %d cannot create local UDP socket. Reason: %s.\n", getpid(), strerror(errno));
		exit(1);
	}
	debug_printf("Router %d created local UDP socket %d.\n", getpid(), *udp_sock_fd);

	/* initialize the socket fields */
	memset((char *) &udp_my_addr, 0, sizeof (udp_my_addr));
	udp_my_addr.sin_family = AF_INET;
	udp_my_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 
	udp_my_addr.sin_port = htons(0); 

	if (-1 == bind(*udp_sock_fd, (struct sockaddr *) &udp_my_addr, sizeof (udp_my_addr)))
	{
		debug_printf("Error! Router %d cannot bind local UDP socket %d. Reason: %s.\n", getpid(), *udp_sock_fd, strerror(errno));
		exit(1);
	}
	debug_printf("Router %d bounded local UDP socket %d.\n", getpid(), *udp_sock_fd);

	if (-1 == getsockname(*udp_sock_fd, (struct sockaddr*) &udp_my_addr, &udp_sock_len))
	{
		debug_printf("Error! Router %d cannot getsockname(...) of local UDP socket &d. Reason: %s.\n", getpid(), *udp_sock_fd, strerror(errno));
		exit(1);
	}
	debug_printf("Router %d did getsockname(...) of local UDP socket %d.\n", getpid(), *udp_sock_fd);

	*udp_port_no = ntohs(udp_my_addr.sin_port);
	debug_printf("Router %d got local UDP socket %d-->%d.\n", getpid(), *udp_sock_fd, *udp_port_no);
}

int create_server_socket()
{
	int server_sock_fd;
	struct sockaddr_in serv_addr;

	if (-1 == (server_sock_fd = socket(AF_INET, SOCK_STREAM, 0)))
	{
		debug_printf("Error! Router %d cannot create server socket. Reason: %s.\n", getpid(), strerror(errno));
		exit(1);
	}
	debug_printf("Router %d created server socket %d.\n", getpid(), server_sock_fd);

	memset(&serv_addr, '0', sizeof (serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 
	serv_addr.sin_port = htons(MANAGER_PORT_NO);

	if (-1 == connect(server_sock_fd, (const struct sockaddr *) &serv_addr, sizeof (serv_addr)))
	{
		debug_printf("Error. Router %d cannot connect to server at socket %d. Reason: %s.\n", getpid(), server_sock_fd, strerror(errno));
		exit(1);
	}

	return server_sock_fd;
}

int create_destination_udp_socket(int dest_udp_port_no)
{
	int dest_udp_sock_fd;
	struct sockaddr_in udp_my_addr;

	/* create the UDP socket */
	if (-1 == (dest_udp_sock_fd = socket(AF_INET, SOCK_DGRAM, 0)))
	{
		debug_printf("Error! Router %d cannot create destination UDP socket. Reason: %s.\n", getpid(), strerror(errno));
		exit(1);
	}
	debug_printf("Router %d created destination UDP socket %d-->%d.\n", getpid(), dest_udp_sock_fd, dest_udp_port_no);

	/* initialize the socket fields */
	memset((char *) &udp_my_addr, 0, sizeof (udp_my_addr));
	udp_my_addr.sin_family = AF_INET;
	udp_my_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 
	udp_my_addr.sin_port = htons(dest_udp_port_no);

	if (-1 == connect(dest_udp_sock_fd, (const struct sockaddr *) &udp_my_addr, sizeof (udp_my_addr)))
	{
		debug_printf("Error. Router %d cannot connect destination UDP socket %d-->%d. Reason: %s.\n", getpid(), dest_udp_sock_fd, dest_udp_port_no, strerror(errno));
		exit(1);
	}

	return dest_udp_sock_fd;
}

void recalculate_best_costs(int my_id, struct router *routers, int **direct_costs, int nrouters)
{
	int from, to;
	bool_t modified;

	do
	{
		modified = FALSE;
		for (from = 0; from < nrouters; ++from)
			for (to = 0; to < nrouters; ++to)
				/* if not self and routers[from] is reachable and routers[from] and routers[to] are neighbors */
				if (to != my_id && routers[from].best_cost < INT_MAX && direct_costs[from][to] < INT_MAX)
					/* if a better cost for routers[to] can be achieved */
					if (routers[from].best_cost + direct_costs[from][to] < routers[to].best_cost)
					{
						routers[to].best_cost = routers[from].best_cost + direct_costs[from][to];
						routers[to].next_hop_id = from == my_id ? to : routers[from].next_hop_id;
						modified = TRUE;
					}
	}
	while (modified);
}

void run_router()
{
	int i;
	int my_id, nrouters;
	char *pch;
	char buffer[BUFFER_SIZE];
	int server_sock_fd;
	int udp_sock_fd;
	int udp_port_no;
	struct router *routers;
	int **direct_costs;

	create_local_udp_socket(&udp_sock_fd, &udp_port_no);
	usleep(1000 * 1000); /* 1 second to make sure the manager has initialized his listening socket */

	server_sock_fd = create_server_socket();

	/* talk to Manager to learn about self and neighbors */
	{
		int from, to;
		sprintf(buffer, "CONFIG %d", udp_port_no);
		write_with_timeout(server_sock_fd, buffer, DEFAULT_TIMEOUT);
		debug_printf("Router %d sent '%s' to socket %d.\n", getpid(), buffer, server_sock_fd);

		read_with_timeout(server_sock_fd, buffer, DEFAULT_TIMEOUT); /* receive SETUP */
		debug_printf("Router %d received '%s' from socket %d.\n", getpid(), buffer, server_sock_fd);

		sscanf(buffer, "SETUP %d %d", &my_id, &nrouters);

		routers = (struct router*) calloc(nrouters, sizeof (struct router));
		for (i = 0; i < nrouters; ++i)
		{
			routers[i].direct_cost = i == my_id ? 0 : INT_MAX;
			routers[i].best_cost = i == my_id ? 0 : INT_MAX;
			routers[i].next_hop_id = i == my_id ? i : -1;
			routers[i].udp_sock_fd = -1;
		}

		direct_costs = (int**) calloc(nrouters, sizeof (int*));
		for (i = 0; i < nrouters; ++i)
			direct_costs[i] = (int*) calloc(nrouters, sizeof (int));

		for (from = 0; from < nrouters; ++from)
			for (to = 0; to < nrouters; ++to)
				direct_costs[from][to] = from == to ? 0 : INT_MAX;

		/* read all neighbors from the SETUP message and initialize them in the routers[] array */
		for (pch = strtok(buffer, " "), i = -3; pch != NULL; pch = strtok(NULL, " "), ++i)
			if (i >= 0)
			{
				int udp_port_no = atoi(pch);
				int direct_cost = atoi(pch = strtok(NULL, " "));

				if (i != my_id && direct_cost != 0)
				{
					routers[i].udp_sock_fd = create_destination_udp_socket(udp_port_no);
					routers[i].direct_cost = direct_cost;
					routers[i].best_cost = routers[i].direct_cost;
					routers[i].next_hop_id = i;
					direct_costs[my_id][i] = routers[i].direct_cost;
					direct_costs[i][my_id] = routers[i].direct_cost;
				}
			}
		if (i != nrouters)
		{
			debug_printf("Error. Router %d(#%d) received %d SETUP arguments instead of %d.\n", getpid(), my_id, i, nrouters);
			exit(1);
		}

		write_with_timeout(server_sock_fd, "READY", DEFAULT_TIMEOUT);
		debug_printf("Router %d(#%d) sent '%s' to socket %d.\n", getpid(), my_id, "READY", server_sock_fd);

		read_with_timeout(server_sock_fd, buffer, DEFAULT_TIMEOUT);
		debug_printf("Router %d(#%d) received '%s' from socket %d.\n", getpid(), my_id, buffer, server_sock_fd);

		if (0 != strncmp(buffer, "BEGIN", 5))
		{
			debug_printf("Error. Router %d(#%d) received '%s' from Manager, but expected BEGIN.\n", getpid(), my_id, buffer);
			exit(1);
		}
	}

	/* flood the network to learn about everyone */
	{
		int ninitialized;
		struct timeval tv_started;

		/* create the LSP message */
		sprintf(buffer, "LSP %d", my_id);
		for (i = 0; i < nrouters; ++i)
			if (i != my_id && routers[i].direct_cost < INT_MAX)
			{
				char buf[10];
				sprintf(buf, " %d %d", i, routers[i].direct_cost);
				strcat(buffer, buf);
			}

		/* if my LSP arrives back to me, I've seen it and it should be discarded*/
		routers[my_id].lsp_received = TRUE;
		ninitialized = 1; /* only self is initialized */

		/* send the LSP message to each neighbor*/
		for (i = 0; i < nrouters; ++i)
			if (i != my_id && routers[i].direct_cost < INT_MAX)
			{
				write_with_timeout(routers[i].udp_sock_fd, buffer, DEFAULT_TIMEOUT);
				debug_printf("Router %d(#%d) sent to #%d LSP message: '%s'\n", getpid(), my_id, i, buffer);
			}
		
		gettimeofday(&tv_started, NULL);

		/* receive LSP message and forward them for half of DEFAULT_TIMEOUT */
		while (to_microseconds(elapsed_since(tv_started)) / 1000 < DEFAULT_TIMEOUT / 2)
		{
			int router_id1, router_id2;

			if (0 == read_with_timeout(udp_sock_fd, buffer, SLICE_TIMEOUT))
				continue;

			debug_printf("Router %d(#%d) received LSP message '%s'.\n", getpid(), my_id, buffer);

			sscanf(buffer, "LSP %d", &router_id1);

			/* if this LSP message was never seen before, forward it first and then record the neighbors of the sender */
			if (!routers[router_id1].lsp_received)
			{
				/* send the LSP message to each neighbor*/
				for (i = 0; i < nrouters; ++i)
					if (i != my_id && routers[i].direct_cost < INT_MAX)
					{
						write_with_timeout(routers[i].udp_sock_fd, buffer, DEFAULT_TIMEOUT);
						debug_printf("Router %d(#%d) forwarded to #%d LSP message: '%s'\n", getpid(), my_id, i, buffer);
					}

				for (pch = strtok(buffer, " "), i = -2; pch != NULL; pch = strtok(NULL, " "), ++i)
					if (i >= 0)
					{
						router_id2 = atoi(pch);
						direct_costs[router_id1][router_id2] = atoi(pch = strtok(NULL, " "));
						direct_costs[router_id2][router_id1] = direct_costs[router_id1][router_id2];
					}
				recalculate_best_costs(my_id, routers, direct_costs, nrouters);

				routers[router_id1].lsp_received = TRUE;
				++ninitialized;
			}
		}
		
		if (ninitialized < nrouters)
			debug_printf("Router %d(#%d) unsuccessfully finished the flooding process with %d unique LSP messages received.\n", getpid(), my_id, ninitialized);
		else
			debug_printf("Router %d(#%d) successfully finished the flooding process with %d unique LSP messages received.\n", getpid(), my_id, ninitialized);
		
		/* flooding is finished, hence direct_costs matrix is no longer needed */
		for (i = 0; i < nrouters; ++i)
			free(direct_costs[i]);
		free(direct_costs);
	}

	read_with_timeout(server_sock_fd, buffer, DEFAULT_TIMEOUT);
	if (0 == strcmp(buffer, "PRINT"))
	{
		FILE *fout;

		debug_printf("Router %d(#%d) received '%s' from Manager.\n", getpid(), my_id, buffer);

		sprintf(buffer, "%d.rtable", my_id);
		fout = fopen(buffer, "w");

		for (i = 0; i < nrouters; ++i)
			fprintf(fout, "%d %d %d\n", i, routers[i].next_hop_id, routers[i].best_cost);

		fclose(fout);
	}

	write_with_timeout(server_sock_fd, "DONE", DEFAULT_TIMEOUT);
	debug_printf("Router %d(#%d) sent '%s' to Manager at socket %d.\n", getpid(), my_id, "DONE", server_sock_fd);


	/* loop expecting one of these:
		- UDP packet from anyone
		- SOURCE message from Manager
		- CLOSE message from Manager */
	while (TRUE)
	{
		/* check for an arrived UDP packet */
		if (0 < read_with_timeout(udp_sock_fd, buffer, SLICE_TIMEOUT))
		{
			int dest_id;
			int packet_no;

			/* if it's a still wondering packet from the flooding process, just discard it */
			if (0 == strncmp(buffer, "LSP", 3))
			{
				debug_printf("Router %d(#%d) received '%s' on UDP port.\n", getpid(), my_id, buffer);
				continue;
			}
			
			flushed_printf("ROUTER #%d received: '%s'\n", my_id, buffer);

			sscanf(buffer, "PACKET #%d DEST=%d", &packet_no, &dest_id);
			if (dest_id == my_id)
			{
				sprintf(buffer, "DESTINATION %d", my_id);
				write_with_timeout(server_sock_fd, buffer, DEFAULT_TIMEOUT);
			}
			else /* resend */
			{
				int next_hop_id = routers[dest_id].next_hop_id;
				write_with_timeout(routers[next_hop_id].udp_sock_fd, buffer, DEFAULT_TIMEOUT);
			}
		}
		
		/* check for message from Manager */
		else if (0 < read_with_timeout(server_sock_fd, buffer, SLICE_TIMEOUT))
		{
			debug_printf("Router %d(#%d) received '%s' from Manager.\n", getpid(), my_id, buffer);
			if (0 == strncmp(buffer, "SOURCE", 6))
			{
				int dest_id, next_hop_id;
				int packet_no;

				sscanf(buffer, "SOURCE dest=%d packet=%d", &dest_id, &packet_no);
				next_hop_id = routers[dest_id].next_hop_id;

				sprintf(buffer, "PACKET #%d DEST=%d", packet_no, dest_id);
				write_with_timeout(routers[next_hop_id].udp_sock_fd, buffer, DEFAULT_TIMEOUT);
			}

			else if (0 == strcmp(buffer, "CLOSE"))
			{
				close(server_sock_fd);
				close(udp_sock_fd);
				for (i = 0; i < nrouters; ++i)
				{
					if (i != my_id && routers[i].udp_sock_fd != -1)
						close(routers[i].udp_sock_fd);
				}
				free(routers);
				break;
			}
		}
	}
}

