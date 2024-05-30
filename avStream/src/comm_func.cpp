#include "comm_func.h"
#include "IPCommon.h"


class WSAInitializer // Winsock Initializer
{
public:
    WSAInitializer()
    {
#if defined _WIN32 || defined _WIN64
        WSADATA wsadata;
        if (WSAStartup(MAKEWORD(2, 2), &wsadata))
        {
            exit(-1);
        }
#endif //defined _WIN32 || defined _WIN64
    }
    ~WSAInitializer()
    {
#if defined _WIN32 || defined _WIN64
        WSACleanup();
#endif //defined _WIN32 || defined _WIN64
    }
private:
};
static WSAInitializer *sWSAInitializer = NULL;
////////////////////////////////////////////////////////////
int _create_tcp_socket(int *sock_fd, const char *host, int port)
{
    if (!sWSAInitializer)
        sWSAInitializer = new WSAInitializer;
    *sock_fd = -1;
	auto _fd = socket(AF_INET,SOCK_STREAM, 0);
	if(_fd < 0)
		return -1;
    
#ifdef WIN32
    unsigned long ul = 1;
	ioctlsocket(_fd, FIONBIO, &ul);
#else
	fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL, 0) | O_NONBLOCK);
#endif

    struct sockaddr_in ser_addr;
    ipaddr_t a;
    if (!IPCommon::u2ip(host, a))
    {
        socket_close(_fd);
        return -1;
    }

    memcpy(&ser_addr.sin_addr, &a, sizeof(struct in_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(port);
    auto ret = connect(_fd, (struct sockaddr *)&ser_addr, sizeof(ser_addr));
    if(ret != 0)
    {
        fd_set fds;
        struct timeval tm = {5, 0};
        FD_ZERO(&fds);
        FD_SET(_fd, &fds);

        if(select(_fd+1, NULL, &fds, NULL, &tm) <= 0)
        {
            socket_close(_fd);
            return -2;
        }
        else
        {            
            int error_num = -1;  
            int opt_len = sizeof(int);  
            getsockopt(_fd, SOL_SOCKET, SO_ERROR, (char*)&error_num, &opt_len);
            if(error_num != 0)
            {
                socket_close(_fd);
                return -2;
            }
        }    
    }

    *sock_fd = _fd;
    
#ifdef WIN32
    ul = 0;
	ioctlsocket(_fd, FIONBIO, &ul);
#else
	fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL, 0) & (~O_NONBLOCK));
#endif
    {
        int opt = 1;
	    int opt_len = sizeof(int);
	    setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, opt_len);
    }

	return 0;
}

int _create_udp_socket(const char *host, int port)
{
    if (!sWSAInitializer)
        sWSAInitializer = new WSAInitializer;
    ipaddr_t a;
    if (!IPCommon::u2ip(host, a))
        return -1;

	auto _fd = socket(AF_INET,SOCK_DGRAM, 0);
	if(_fd < 0)
		return -1;
    sockaddr_in ser_addr = {};
    memcpy(&ser_addr.sin_addr, &a, sizeof(struct in_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(port);
    if (bind(_fd, (struct sockaddr *)&ser_addr, sizeof(ser_addr)) < 0)
    {
        socket_close(_fd);
        return -1;
    }
    
	return _fd;
}

int _parseAddressAndCreateUdp(struct sockaddr_in *ser_addr, const char *host, int port)
{
    if (!sWSAInitializer)
        sWSAInitializer = new WSAInitializer;
    ipaddr_t a;
    if (!IPCommon::u2ip(host, a))
        return -1;

    memcpy(&ser_addr->sin_addr, &a, sizeof(struct in_addr));
    ser_addr->sin_family = AF_INET;
    ser_addr->sin_port = htons(port);
    auto _fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (_fd < 0)
    {
        socket_close(_fd);
        return -1;
    }

    return _fd;
}

int tcp_write_msg(int fd, char *msg, int len)
{
    int ret = 0;
    int count = 0;
    struct timeval tv= {0, 1000*100};

    ret = send(fd, msg, len, 0);
    while(ret == 0)
    {
        tv.tv_sec = 0;
        tv.tv_usec = 1000*100;
        if(count == 3)
            return -1;

        if(select(0, NULL, NULL, NULL, &tv) == 0)
            ret = send(fd, msg, len, 0);
    }
    
    if(ret < 0)
    {
        return -1;
    }    

	return 0;
}

int tcp_recv_msg(int fd, char *msg, int len)
{
    int ret;
    struct timeval time_out= {2, 0};
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    
    if(select(fd+1, &rfds, NULL, NULL, &time_out) <= 0)
        return -17;

    ret = recv(fd, msg, len, 0);
    if(ret <= 0)
        return -17;

	return ret;
}

int udp_write_msg(int fd, char *msg, int len)
{
	return 0;
}

int udp_recv_msg(int fd, char *msg, int len)
{
	return 0;
}