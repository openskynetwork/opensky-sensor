/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_PORT_SOCKET_H
#define _HAVE_PORT_SOCKET_H

#ifdef HAVE_SYS_SOCKET_H

#include <sys/socket.h>
#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int sock_t;
#define SOCK_INVALID -1
#define SOCK_ERROR -1

#define SOCK_init() do {} while(0)

#define SOCK_cleanup() do {} while(0)

#define SOCK_connect(sock, sockaddr, len) connect((sock), (sockaddr), (len))

#define SOCK_socket(af, type, protocol) socket((af), (type), (protocol))

#define SOCK_getError() errno

#define SOCK_recv(sock, buf, len, flags) recv((sock), (buf), (len), (flags))

#define SOCK_send(sock, buf, len, flags) \
	send((sock), (buf), (len), (flags) | MSG_NOSIGNAL)

#define SOCK_shutdown(sock, how) shutdown((sock), (how))

#define SOCK_close(sock) close((sock))

#ifdef __cplusplus
}
#endif

#elif HAVE_WINSOCK2_H

#include <winsock2.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHUT_RD SD_RECEIVE
#define SHUT_WD SD_SEND
#define SHUT_RDWR SD_BOTH

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

typedef SOCKET sock_t;
#define SOCK_INVALID INVALID_SOCKET
#define SOCK_ERROR SOCKET_ERROR

static inline void SOCK_init()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

#define SOCK_cleanup() WSACleanup()

#define SOCK_connect(sock, sockaddr, len) connect((sock), (sockaddr), (len))

#define SOCK_socket(af, type, protocol) socket((af), (type), (protocol))

#define SOCK_getError() WSAGetLastError()

static inline ssize_t SOCK_recv(sock_t sock, void * buf, size_t len, int flags)
{
	return recv(sock, (char*)buf, (int)len, flags);
}

static inline ssize_t SOCK_send(sock_t sock, const void * buf, size_t len,
	int flags)
{
	return send(sock, (const char*)buf, (int)len, flags);
}

#define SOCK_shutdown(sock, how) shutdown((sock), (how))

#define SOCK_close(sock) closesocket((sock))

#ifdef __cplusplus
}
#endif

#else

#error Neither POSIX nor WINSOCK headers found

#endif

#endif
