#ifndef __COMM_FUNC__H__
#define __COMM_FUNC__H__

#include "rtsp_os.h"

#ifdef __cplusplus
extern "C" {
#endif
    int _create_tcp_socket(int *sock_fd, const char *host, int port);
    int _create_udp_socket(const char *host, int port);
    int _parseAddressAndCreateUdp(struct sockaddr_in *ser_addr, const char *host, int port);

    int tcp_write_msg(int fd, char *msg, int len);
    int tcp_recv_msg(int fd, char *msg, int len);
    int udp_write_msg(int fd, char *msg, int len);
    int udp_recv_msg(int fd, char *msg, int len);
#ifdef __cplusplus
}
#endif

#endif