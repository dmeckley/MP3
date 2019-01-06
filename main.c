// File: main.c
// Programmer: Dustin Meckley
// Date: 12/10/2015

#include "simulator.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

bool_t replicate(int nrouters)
{
	int pid, my_id;

	for (my_id = 0; my_id < nrouters; ++my_id)
		switch (pid = fork())
		{
			case -1:
				debug_printf("Error! Unable to fork.");
				exit(1);

			case 0: /* child */
				return FALSE;

			default: /* parent */
				/* do nothing here */
				break;
		}

	return TRUE;
}

int main(int argc, char** argv)
{
	FILE *finTopology, *finPackets;
	int **neighbors;
	int nrouters;
	int i, j;
	int from, to, cost;

	if (argc < 2)
	{
		flushed_printf("  Usage1: ./ls-sim <configuration_file>\n");
		flushed_printf("  Usage2: ./ls-sim <configuration_file> <packets_file>\n");
		flushed_printf("Example1: ./ls-sim network.txt\n");
		flushed_printf("Example2: ./ls-sim network.txt packets.txt\n");
		return 0;
	}
	finPackets = argc == 3 ? fopen(argv[2], "r") : NULL;
	finTopology = fopen(argv[1], "r");
	fscanf(finTopology, "%d", &nrouters);

	/* allocate memory for the cost matrix all initialized by calloc(...) to 0s (meaning no connection) */
	neighbors = (int**) malloc(nrouters * sizeof (int*));
	for (i = 0; i < nrouters; ++i)
		neighbors[i] = (int*) calloc(nrouters, sizeof (int));

	/* read the configuration from the file */
	while (3 == fscanf(finTopology, "%d %d %d", &from, &to, &cost))
	{
		neighbors[from][to] = cost;
		neighbors[to][from] = cost;
	}

	fclose(finTopology);

	if (replicate(nrouters))
		run_manager(nrouters, neighbors, finPackets);
	else
		run_router();
	
	for (i = 0; i < nrouters; ++i)
		free(neighbors[i]);
	free(neighbors);

	if (finPackets)
		fclose(finPackets);
	
	return 0;
}

