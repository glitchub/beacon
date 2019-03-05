// Ethernet beacon utility

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <ctype.h>
#include <unistd.h>
#include <regex.h>

#define warn(...) fprintf(stderr,__VA_ARGS__)
#define die(...) warn(__VA_ARGS__), exit(1)

static void usage(void)
{
    die("\
Usage:\n\
\n\
    beacon send [-N] interface [message]\n\
\n\
Send ethernet beacons on the specified interface, forever until killed.\n\
\n\
Where:\n\
    \n\
    \"-N\" is the repeat rate in seconds, default 1.\n\
\n\
    \"message\" is up to 1498 characters to be sent in the beacon payload. The\n\
    default message is \"beacon\". \n\
\n\
-or-\n\
\n\
    beacon recv [-N] [regex]\n\
\n\
Wait for an ethernet beacon on any interface, print the contained message and\n\
exit 0, or exit 1 on timeout.\n\
\n\
Where:\n\
\n\
    -N is the number of seconds to wait, default 5. If 0 then never timeout,\n\
    just print received beacons forever.\n\
\n\
    \"regex\" is a regular expression to match in the beacon message. Non\n\
    matching beacons are ignored.  Only the part of the message that matches\n\
    the regex will be printed (case-insensitive). The default regex is \".*$\",\n\
    i.e. match any beacon.\n\
"   );
}

struct payload {
    uint16_t bytes;
    int8_t message[1498];
};

int debug=0;

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "-d")) debug=1, argc--, argv++;
    if (argc < 2) usage();
    if (!strcmp(argv[1],"send"))
    {
        // send beacon
        struct payload p;
        struct sockaddr_ll addr;
        struct ifreq ifr;
        char *message="beacon", *interface;
        int sock, seconds=0;

        // parse '[-N] interface [message]'
        if (argc < 3) usage();
        if (*argv[2] == '-') seconds=atoi(argv[2]+1), argc--, argv--;
        if (argc < 3) usage();
        interface=argv[2];
        if (argc > 3) message=argv[3];

        // Create payload
        if (strlen(message) > sizeof(p.message)) die("Message is too long\n");
        p.bytes=htons(strlen(message)^0xBEAC);
        memcpy(p.message, message, strlen(message));

        // Create send socket
        if ((sock = socket(AF_PACKET, SOCK_DGRAM, 0)) < 0) die("socket failed: %s\n", strerror(errno));

        // Get interface index
        if (!*interface || strlen(interface) >= sizeof(ifr.ifr_name)) die("Interface name is invalid\n");
        strcpy(ifr.ifr_name, interface);
        if (ioctl(sock, SIOCGIFINDEX, &ifr) == -1) die("SIOCGIFINDEX on %s failed: %s\n", ifr.ifr_name,strerror(errno));

        // Create broadcast address
        addr.sll_family = AF_PACKET;
        addr.sll_ifindex=ifr.ifr_ifindex;
        addr.sll_halen=ETHER_ADDR_LEN;
        addr.sll_protocol=htons(0xBEAC);
        memset(addr.sll_addr,0xff,ETH_ALEN);

        while(1)
        {
            // send the packet over the socket
            if (sendto(sock, &p, strlen(message)+2, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) die("sendto failed: %s\n", strerror(errno));
            sleep((seconds>0) ? (unsigned)seconds : 1);
        }

    } else if (!strcmp(argv[1], "recv"))
    {
        // receive beacon
        int sock, seconds=4;
        char *message=".*$";
        fd_set fds;
        struct timeval t;
        regex_t regex;

        // parse '[-N] [message]'
        if (argc > 2)
        {
            if (*argv[2] == '-') seconds=atoi(argv[2]+1), argc--, argv++;
            if (argc > 2) message=argv[3];
        }

        // set the timeout
        t.tv_sec=seconds;
        t.tv_usec=0;

        // compile the regex
        int r=regcomp(&regex, message, REG_NEWLINE|REG_ICASE);
        if (r)
        {
            char buf[256];
            regerror(r, &regex, buf, sizeof buf);
            die("Invalid regex '%s': %s\n", message, buf);
        }

        if (debug) warn("Waiting for '%s'\n", message);

        // Create socket that listens for ether type 0xBEAC. Yes it's undefined, that's kind of the point.
        if ((sock = socket(AF_PACKET, SOCK_DGRAM, htons(0xBEAC))) < 0) die("socket failed: %s\n", strerror(errno));

        FD_ZERO(&fds);
        FD_SET((unsigned)sock, &fds);

        while(1)
        {
            struct payload p;
            uint16_t bytes;
            int got;
            char *s;
            regmatch_t match;

            // maybe wait for timeout
            if (seconds && select(sock+1, &fds, NULL, NULL, &t)<=0) return 1; // exit on timeout (or error)

            // receive a packet, should be 46 to 1500 bytes
            if ((got=recvfrom(sock, &p, sizeof(p), 0, NULL, NULL))<46) die("recvfrom failed: %s\n", strerror(errno));
            bytes=ntohs(p.bytes)^0xBEAC;
            if (bytes > got-2)
            {
                if (debug) warn("Ignoring invalid payload bytes=%d got=%d\n", bytes, got);
                continue;
            }

            if (asprintf(&s, "%.*s", bytes, p.message) < 0) die("Out of memory!\n");
            if (!regexec(&regex,s,3,&match,0))
            {
                printf("%.*s\n", match.rm_eo-match.rm_so, &s[match.rm_so]);
                if (seconds) return 0;
            } else {
                if (debug) warn("Ignoring unexpected message '%s'\n", s);
            }
            free(s);
        }
    }
    usage();
    return 1; // won't get here
}
