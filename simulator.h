// File: simulator.h
// Programmer: Dustin Meckley
// Date: 12/10/2015

#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <stdio.h>
#include <sys/time.h>

#define FALSE 0
#define TRUE 1
#define DEBUG 0
#define BUFFER_SIZE 1024
#define MANAGER_PORT_NO 8888
#define DEFAULT_TIMEOUT 4000 /* milliseconds */
#define SLICE_TIMEOUT 100 /* milliseconds */

typedef int bool_t;

int flushed_printf(const char * format, ...);

int debug_printf(const char * format, ...);

long long to_microseconds(const struct timeval tv1);

struct timeval from_microseconds(long long microseconds);

struct timeval diff_timeval(const struct timeval tv1, const struct timeval tv2);

struct timeval elapsed_since(const struct timeval tv1);

/* reads repeatedly during the allowed time until the whole message has been read */
int read_with_timeout(int sock, char buffer[], int milliseconds);

/* writes repeatedly during the allowed time until the whole message has been written */
int write_with_timeout(int socket, char buffer[], int milliseconds);

void run_manager(int nrouters, int **neighbors, FILE *finPackets);

void run_router();

#endif /* SIMULATOR_H */

