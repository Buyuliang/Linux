#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h> 
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <asm/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/route.h>


 
#define BUFLEN 2*1024
#define MSG_BUF_LEN 1*1024
 
#define t_assert(x) { \
	if(x)  {ret = -__LINE__;goto error;} \
}
 
/*Ctrl + C exit*/
static volatile int keepRunning = 1;
 
void intHandler(int dummy)
{
	keepRunning = 0;
}
 
/*
 * decode RTA, save into tb
 */
void parse_rtattr(struct rtattr **tb, int max, struct rtattr *attr, int len)
{
	for (; RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
		if (attr->rta_type <= max) {
			tb[attr->rta_type] = attr;
		}
	}
}
 
/*
 * show link information
 * triggered when netinterface link state changed
 * like add/del Ethertnet cable, add/del NIC,
 * enable/disable netinterface etc.
 */
void print_ifinfomsg(struct nlmsghdr *nlh)
{
	int len;
	struct rtattr *tb[IFLA_MAX + 1];
	struct ifinfomsg *ifinfo;
 
	bzero(tb, sizeof(tb));
	ifinfo = NLMSG_DATA(nlh);
	len = IFLA_PAYLOAD(nlh);
	printf("PAYLOAD:%d\n", len);
	printf("nlmsg_len:%d, nlmsg_seq:%d, nlmsg_pid:%d\n", nlh->nlmsg_len, nlh->nlmsg_seq, nlh->nlmsg_pid);
	parse_rtattr(tb, IFLA_MAX, IFLA_RTA (ifinfo), len);
 
	printf("%s: %s ", (nlh->nlmsg_type==RTM_NEWLINK)?"NEWLINK":"DELLINK",
		(ifinfo->ifi_flags & IFF_UP) ? "up" : "down");
 
	if(tb[IFLA_IFNAME])
		printf("%s\t", RTA_DATA(tb[IFLA_IFNAME]));
	printf("\n");
}

char OLDADDR[IFNAMSIZ];
/*
 * show IP address information
 * triggered when  IP address changed
 */
void print_ifaddrmsg(struct nlmsghdr *nlh)
{
	int len;
	struct rtattr *tb[IFA_MAX + 1];
	struct ifaddrmsg *ifaddr;
	char netseg[IFNAMSIZ];
	char gateway[IFNAMSIZ];
	char cmd[100];
	uint32_t ipaddr, mask;
 	struct sockaddr_in serv;    
    memset(&serv,0,sizeof(struct sockaddr_in));
    serv.sin_family = AF_INET;

	bzero(tb, sizeof(tb));
	ifaddr = NLMSG_DATA(nlh);
	len = IFA_PAYLOAD(nlh);
	// printf("PAYLOAD:%d\n", len);
	parse_rtattr(tb, IFA_MAX, IFA_RTA (ifaddr), len);
 
	printf("%s ", (nlh->nlmsg_type==RTM_NEWADDR)?"NEWADDR":"DELADDR");
	printf("\n");
	if (tb[IFA_LABEL]) {
		// printf("%s ", RTA_DATA(tb[IFA_LABEL]));
	}
	if (tb[IFA_ADDRESS]) {
		inet_ntop(ifaddr->ifa_family, RTA_DATA(tb[IFA_ADDRESS]), gateway, sizeof(gateway));
		printf("%s \n", gateway);
	}
	ipaddr = htonl(*((uint32_t *)RTA_DATA(tb[IFA_ADDRESS])));
	mask = (~(uint32_t)(pow(2, 32 - ifaddr->ifa_prefixlen) - 1));
	serv.sin_addr.s_addr = ntohl(ipaddr & mask);
	printf("net addr: %x\n", mask);
	inet_ntop(AF_INET, &serv.sin_addr.s_addr, netseg, sizeof(netseg));
	printf("mask: %s ", netseg);
    if (nlh->nlmsg_type == RTM_NEWADDR) {
        if (strcmp(OLDADDR, gateway) != 0) {
			sprintf(cmd, "echo add %s/%u via %s dev wlan0 table 1009", netseg, ifaddr->ifa_prefixlen, gateway);
			system(cmd);
            printf("del route xxx\n");
            printf("add route xxx\n\n");
            strcpy(OLDADDR, gateway);
        }
    }
	printf("mask: %u\n",ifaddr->ifa_prefixlen);
	printf("\n");
}
 
/*
 * show route information
 * tiggered when route changed
 */
void print_rtmsg(struct nlmsghdr *nlh)
{
	int len;
	struct rtattr *tb[RTA_MAX + 1];
	struct rtmsg *rt;
	char tmp[20];
 
	bzero(tb, sizeof(tb));
	rt = NLMSG_DATA(nlh);
	len = RTM_PAYLOAD(nlh);
	printf("PAYLOAD:%d\n", len);
	parse_rtattr(tb, RTA_MAX, RTM_RTA(rt), len);
 
	printf("%s: ", (nlh->nlmsg_type==RTM_NEWROUTE)?"NEWROUTE":"DELROUTE");
 
	if (tb[RTA_DST]) {
		inet_ntop(rt->rtm_family, RTA_DATA(tb[RTA_DST]), tmp, sizeof(tmp));
		printf("RTA_DST %s ", tmp);
	}
	if (tb[RTA_SRC]) {
		inet_ntop(rt->rtm_family, RTA_DATA(tb[RTA_SRC]), tmp, sizeof(tmp));
		printf("RTA_SRC %s ", tmp);
	}
	if (tb[RTA_GATEWAY]) {
		inet_ntop(rt->rtm_family, RTA_DATA(tb[RTA_GATEWAY]), tmp, sizeof(tmp));
		printf("RTA_GATEWAY %s ", tmp);
	}
 
	printf("\n");
}
int main(int argc, char *argv[])
{
	int socket_fd;
	int ret = 0;
 
	/* select() used */
/*
	fd_set rd_set;
	struct timeval timeout;
	int select_r;
*/
	struct sockaddr_nl my_addr;
	struct sockaddr_nl peer_addr;
	struct nlmsghdr *nlh = NULL;
	struct nlmsghdr *nh = NULL;
	struct msghdr msg;
	struct iovec iov;
	int len = 0;
 
	signal(SIGINT, intHandler);
    printf("start: socket ... \n");
	/* open netlink socket */
	socket_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	t_assert((socket_fd > 0) ? 0 : socket_fd);
 
	/* set socket option (receive buff size) or you can use default size*/
/*
	int skb_len = BUFLEN;
	int optvalue;
	socklen_t optlen;
	getsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &optvalue, &optlen);
	printf("default socket recvbuf optvalue:%d, optlen:%d\n", optvalue, optlen);
	t_assert(setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &skb_len, sizeof(skb_len)));
	getsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &optvalue, &optlen);
	printf("set socket recvbuf optvalue:%d, optlen:%d\n", optvalue, optlen);
*/
 
	/*set recvmsg type and bind socket*/
	bzero(&my_addr, sizeof(my_addr));
	my_addr.nl_family = AF_NETLINK;
	my_addr.nl_pid = getpid();
	my_addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE;
	t_assert(bind(socket_fd, (struct sockaddr *) &my_addr, sizeof(my_addr)));
 
 
	/* fill msghdr */
	nlh = (struct nlmsghdr*)malloc(MSG_BUF_LEN);
	t_assert(!nlh);
	nh = nlh;
	iov.iov_base = (void *)nlh;
	iov.iov_len = MSG_BUF_LEN;
	bzero(&msg, sizeof(msg));
	/* For recvmsg,this domain is used to
	 * save the peer sockaddr info, init to NULL to ignore this domain
	 * or malloc memory for it to catch && save the peer sockaddr info
	 *
	 * ignore
	 * msg.msg_name = NULL;
	 * msg.msg_namelen = 0;
	 *
	 * catch && save
	 * msg.msg_name = (void *)&peer_addr;
	 * msg.msg_namelen = sizeof(peer_addr);
	 *
	 *
	 * For sendmsg this domain should be filled with the peer sockaddr info
	 * (also could be ignored)
	 *
	 * for connected communication like TCP
	 * this domain could be inited to NULL(ignore)
	 *
	 * for none connected communication like UDP
	 * this domain should be filled with the peer sockaddr info
	 * msg.msg_name = (void *)&peer_addr;
	 * msg.msg_namelen = sizeof(peer_addr);
	 */
	bzero(&peer_addr,sizeof(peer_addr));
	msg.msg_name = &peer_addr;
	msg.msg_namelen = sizeof(peer_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
 
/* recvmsg block way
 *
 * recvmsg() revceive msg using block way by default(flags is set to 0)
 */
	while (keepRunning) {
		len = recvmsg(socket_fd, &msg, 0);
		// printf("peer_addr.nl_family:%d, peer_addr.nl_pid:%d\n", ((struct sockaddr_nl *)(msg.msg_name))->nl_family, ((struct sockaddr_nl *)(msg.msg_name))->nl_pid);
		// printf("MSG_LEN:%d\n", len);
		for (nlh = nh; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len)) {
			switch (nlh->nlmsg_type) {
			default:
				printf("unknown msg type\n");
				printf("nlh->nlmsg_type = %d\n", nlh->nlmsg_type);
				break;
			case NLMSG_DONE:
			case NLMSG_ERROR:
				break;
			case RTM_NEWLINK:
			case RTM_DELLINK:
				// print_ifinfomsg(nlh);
				break;
			case RTM_NEWADDR:
			case RTM_DELADDR:
				print_ifaddrmsg(nlh);
				break;
			case RTM_NEWROUTE:
			case RTM_DELROUTE:
				// print_rtmsg(nlh);
				break;
			}
		}
	}
 
/* receive msg none-block way
 *
 * by using select()
 */
 
/*
	while (keepRunning) {
		FD_ZERO(&rd_set);
		FD_SET(socket_fd, &rd_set);
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		select_r = select(socket_fd + 1, &rd_set, NULL, NULL, &timeout);
		if (select_r < 0) {
			perror("select");
		} else if (select_r > 0) {
			if (FD_ISSET(socket_fd, &rd_set)) {
				len = recvmsg(socket_fd, &msg, 0);
				printf("MSG_LEN:%d\n", len);
				for (nlh = nh; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len)) {
					switch (nlh->nlmsg_type) {
					default:
						printf("unknown msg type\n");
						printf("nlh->nlmsg_type = %d\n", nlh->nlmsg_type);
						break;
					case NLMSG_DONE:
					case NLMSG_ERROR:
						break;
					case RTM_NEWLINK:
					case RTM_DELLINK:
						print_ifinfomsg(nlh);
						break;
					case RTM_NEWADDR:
					case RTM_DELADDR:
						print_ifaddrmsg(nlh);
						break;
					case RTM_NEWROUTE:
					case RTM_DELROUTE:
						print_rtmsg(nlh);
						break;
					}
				}
			}
			bzero(nh, MSG_BUF_LEN);
		}
	}
*/
	close(socket_fd);
	free(nh);
 
error:
	if (ret < 0) {
		printf("Error at line %d\nErrno=%d\n", -ret, errno);
	}
	return ret;
}
