// File: simulator.c
// Programmer: Dustin Meckley
// Date: 12/10/2015

#include "simulator.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>

int flushed_printf(const char * format, ...)
{
	int result;
	va_list argptr;
	va_start(argptr, format);

	result = vprintf(format, argptr);
	fflush(stdout);

	return result;
}

int debug_printf(const char * format, ...)
{
	if (DEBUG)
	{
		int result;
		va_list argptr;
		va_start(argptr, format);

		result = vprintf(format, argptr);
		fflush(stdout);

		return result;
	}
}

long long to_microseconds(const struct timeval tv1)
{
	return 1000000LL * (long long) tv1.tv_sec + (long long) tv1.tv_usec;
}

struct timeval from_microseconds(long long microseconds)
{
	struct timeval tv;

	tv.tv_sec = microseconds / 1000000;
	tv.tv_usec = microseconds % 1000000;

	return tv;
}

struct timeval diff_timeval(const struct timeval tv1, const struct timeval tv2)
{
	return from_microseconds(to_microseconds(tv1) - to_microseconds(tv2));
}

struct timeval elapsed_since(const struct timeval tv1)
{
	struct timeval tv2;
	gettimeofday(&tv2, NULL);

	return diff_timeval(tv2, tv1);
}

/* reads repeatedly during the allowed time until the whole message has been read */
int read_with_timeout(int sock, char buffer[], int milliseconds)
{
	long long msg_len, nremaining; 
	struct timeval tv_start;
	struct timeval tv_maxtime = from_microseconds(1000 * milliseconds);
	struct timeval tv_remaining = tv_maxtime;
	int socket_type;
	int type_length = sizeof (int);

	if (-1 == setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_remaining, sizeof (tv_remaining)))
	{
		debug_printf("Error in %d setting socket %d with setsockopt. Reason: %s\n", getpid(), sock, strerror(errno));
		return -1;
	}

	getsockopt(sock, SOL_SOCKET, SO_TYPE, &socket_type, &type_length);

	/* if TCP then receive the message length as a 64-bit integer before the actual message */
	if (socket_type == SOCK_STREAM)
	{
		if (sizeof (long long) != recv(sock, &msg_len, sizeof (long long), 0))
		{
			if (errno == 0 || errno == EAGAIN) /* timed out */
			{
				buffer[0] = 0;
				return 0;
			}
			debug_printf("Error in %d reading message length from socket %d. Reason: %s\n", getpid(), sock, strerror(errno));
			return -1;
		}

		if (msg_len > BUFFER_SIZE)
		{
			debug_printf("Message length in buffer of receiver %d is too high, msg_len = %d\n", getpid(), (int) msg_len);
			return -1;
		}

		gettimeofday(&tv_start, NULL);

		nremaining = msg_len;
		while (nremaining > 0)
		{
			int n;

			tv_remaining = to_microseconds(elapsed_since(tv_start)) < to_microseconds(tv_maxtime) ? diff_timeval(tv_maxtime, elapsed_since(tv_start)) : from_microseconds(0);

			if (-1 == setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_remaining, sizeof (tv_remaining)))
			{
				debug_printf("Error in %d with second setting socket %d with setsockopt and SO_RCVTIMEO. Reason: %s\n", getpid(), sock, strerror(errno));
				return -1;
			}

			if (-1 == (n = recv(sock, buffer + msg_len - nremaining, nremaining, 0)))
			{
				if (errno == 0 || errno == EAGAIN) /* timed out */
				{
					buffer[0] = 0;
					return 0;
				}
				debug_printf("Error in %d reading message from socket %d. Reason: %s\n", getpid(), sock, strerror(errno));
				return -1;
			}
			nremaining -= n;
		}
	}
	else /* expecting a UDP packet */
	{
		if (-1 == (msg_len = recv(sock, buffer, BUFFER_SIZE - 1, 0)))
		{
			if (errno == 0 || errno == EAGAIN) /* timed out */
			{
				buffer[0] = 0;
				return 0;
			}
			debug_printf("Error in %d reading message from UDP socket %d. Reason: %s\n", getpid(), sock, strerror(errno));
			return -1;
		}
	}

	buffer[msg_len] = 0;

	return msg_len;
}

/* writes repeatedly during the allowed time until the whole message has been written */
int write_with_timeout(int socket, char buffer[], int milliseconds)
{
	long long msg_len, nremaining;
	struct timeval tv_start;
	struct timeval tv_maxtime = from_microseconds(1000 * milliseconds);
	struct timeval tv_remaining = tv_maxtime;
	int socket_type;
	int type_length = sizeof (int);

	gettimeofday(&tv_start, NULL);

	if (BUFFER_SIZE < (msg_len = strlen(buffer)))
	{
		debug_printf("Message length in buffer of sender %d is too high, msg_len = %d\n", getpid(), (int) msg_len);
		return -1;
	}

	if (-1 == setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv_remaining, sizeof (tv_remaining)))
	{
		debug_printf("Error in %d first setting of socket %d with setsockopt and SO_SNDTIMEO. Reason: %s\n", getpid(), socket, strerror(errno));
		return -1;
	}

	getsockopt(socket, SOL_SOCKET, SO_TYPE, &socket_type, &type_length);

	/* if TCP then send the message length before the actual message */
	if (socket_type == SOCK_STREAM)
		if (sizeof (long long) != send(socket, &msg_len, sizeof (long long), 0))
		{
			if (errno == 0 || errno == EAGAIN) /* timed out */
				return 0;
			debug_printf("Error in %d writing message length to socket %d. Reason: %s\n", getpid(), socket, strerror(errno));
			return -1;
		}

	/* send the message in chunks until it's all sent */
	nremaining = msg_len;
	while (nremaining > 0)
	{
		int n;

		tv_remaining = to_microseconds(elapsed_since(tv_start)) < to_microseconds(tv_maxtime) ? diff_timeval(tv_maxtime, elapsed_since(tv_start)) : from_microseconds(0);

		if (-1 == setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv_remaining, sizeof (tv_remaining)))
		{
			debug_printf("Error in %d with second setting of socket %d with setsockopt and SO_SNDTIMEO. Reason: %s\n", getpid(), socket, strerror(errno));
			return -1;
		}

		if (-1 == (n = send(socket, buffer + msg_len - nremaining, nremaining, 0)))
		{
			if (errno == 0 || errno == EAGAIN) /* timed out */
				return 0;
			debug_printf("Error in %d writing message to socket %d. Reason: %s\n", getpid(), socket, strerror(errno));
			return -1;
		}

		nremaining -= n;
	}

	return msg_len;
}

