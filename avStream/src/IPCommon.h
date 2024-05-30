#ifndef _IP_COMMON_H
#define _IP_COMMON_H

#include <string>
#include "rtsp_os.h"

namespace IPCommon {
	/** Checks whether a string is a valid ipv4/ipv6 ip number. */
    bool isipv4(const std::string&);
	/** Checks whether a string is a valid ipv4/ipv6 ip number. */
    bool isipv6(const std::string&);

	/** Hostname to ip resolution ipv4, not asynchronous. */
    bool u2ip(const std::string&, ipaddr_t&);
    bool u2ip(const std::string&, struct sockaddr_in& sa, int ai_flags = 0);
#ifdef ENABLE_IPV6
#ifdef IPPROTO_IPV6
	/** Hostname to ip resolution ipv6, not asynchronous. */
    bool u2ip(const std::string&, struct in6_addr&);
#endif
#endif
	/** Reverse lookup of address to hostname */
    bool reverse(struct sockaddr *sa, socklen_t sa_len, std::string&, int flags = 0);
    bool reverse(struct sockaddr *sa, socklen_t sa_len, std::string& hostname, std::string& service, int flags = 0);

	/** Convert binary ip address to string: ipv4. */
    void l2ip(const ipaddr_t,std::string& );
    void l2ip(const in_addr&,std::string& );
#ifdef ENABLE_IPV6
#ifdef IPPROTO_IPV6
	/** Convert binary ip address to string: ipv6. */
    SHARED_DECL void l2ip(const struct in6_addr&,std::string& ,bool mixed = false);

	/** ipv6 address compare. */
    SHARED_DECL int in6_addr_compare(in6_addr,in6_addr);
#endif
#endif
	/** ResolveLocal (hostname) - call once before calling any GetLocal method. */
    void ResolveLocal();
	/** Returns local hostname, ResolveLocal must be called once before using.
		\sa ResolveLocal */
    const std::string& GetLocalHostname();
	/** Returns local ip, ResolveLocal must be called once before using.
		\sa ResolveLocal */
    ipaddr_t GetLocalIP();
	/** Returns local ip number as string.
		\sa ResolveLocal */
    const std::string& GetLocalAddress();
#ifdef ENABLE_IPV6
#ifdef IPPROTO_IPV6
	/** Returns local ipv6 ip.
		\sa ResolveLocal */
    SHARED_DECL const struct in6_addr& GetLocalIP6();
	/** Returns local ipv6 address.
		\sa ResolveLocal */
    SHARED_DECL const std::string& GetLocalAddress6();
#endif
#endif
}

#endif // _SOCKETS_Utility_H

