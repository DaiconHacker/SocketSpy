// gcc -Wall -O3 -static ./socketspy.c -o socketspy

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <linux/netlink.h>
#include <linux/inet_diag.h>
#include <linux/sock_diag.h>
#include <fcntl.h>
#include <stdarg.h>

#define MAX_EVENTS 4
#define HASH_SIZE 65536
#define TCPF_ALL 0xFFF
#define INTERVAL_MICROSEC 1000

int logfd = -1;
static long long run_deadline_ns = -1;

static long long monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

static long long remaining_ns(void) {
    if (run_deadline_ns < 0) {
        return -1;
    }

    long long rem = run_deadline_ns - monotonic_ns();
    return (rem > 0) ? rem : 0;
}

void dual_output(const char *fmt, ...)
{
    char buf[2048];

    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len > 0) {
        if (logfd >= 0) {
            write(logfd, buf, len);
        } else{
            write(STDOUT_FILENO, buf, len);
        }
    }
}

/* =========================
  Connection key (for deduplication)
   ========================= */
typedef struct {
    uint8_t proto;
    uint32_t lip, rip;
    uint16_t lport, rport;
} conn_key_t;

/* =========================
         Hash table
   ========================= */
typedef struct node {
    conn_key_t key;
    struct node *next;
} node_t;

node_t *htable[HASH_SIZE];

unsigned int hash_conn(conn_key_t *k) {
    return (k->proto ^ k->lip ^ k->rip ^ k->lport ^ k->rport) % HASH_SIZE;
}

int is_duplicate(conn_key_t *k) {
    unsigned int h = hash_conn(k);
    node_t *n = htable[h];
    while (n) {
        if (memcmp(&n->key, k, sizeof(conn_key_t)) == 0)
            return 1;
        n = n->next;
    }
    n = malloc(sizeof(node_t));
    n->key = *k;
    n->next = htable[h];
    htable[h] = n;
    return 0;
}

/* =========================
   inode → PID
   ========================= */
int find_pid_by_inode(unsigned long inode, pid_t *pid_out, char *cmd, size_t sz) {
    DIR *proc = opendir("/proc");
    if (!proc) return 0;

    struct dirent *ent;
    while ((ent = readdir(proc))) {
        if (!isdigit(ent->d_name[0])) continue;

        pid_t pid = atoi(ent->d_name);

        char path[256];
        snprintf(path, sizeof(path), "/proc/%d/fd", pid);

        DIR *fd = opendir(path);
        if (!fd) continue;

        struct dirent *f;
        while ((f = readdir(fd))) {
            if (f->d_name[0] == '.') continue;

            char lpath[512], target[512];
            snprintf(lpath, sizeof(lpath), "%s/%s", path, f->d_name);

            ssize_t len = readlink(lpath, target, sizeof(target)-1);
            if (len < 0) continue;
            target[len] = 0;

            unsigned long ino;
            if (sscanf(target, "socket:[%lu]", &ino) == 1) {
                if (ino == inode) {
                    *pid_out = pid;

                    char cmdpath[256];
                    snprintf(cmdpath, sizeof(cmdpath), "/proc/%d/cmdline", pid);
                    FILE *fp = fopen(cmdpath, "r");
                    if (fp) {
                        size_t r = fread(cmd, 1, sz-1, fp);
                        fclose(fp);
                        for (size_t i=0;i<r;i++) if (cmd[i]==0) cmd[i]=' ';
                        cmd[r]=0;
                    } else cmd[0]=0;

                    closedir(fd);
                    closedir(proc);
                    return 1;
                }
            }
        }
        closedir(fd);
    }
    closedir(proc);
    return 0;
}

/* =========================
             Time
   ========================= */
void now(char *buf, size_t sz) {
    time_t t = time(NULL);
    strftime(buf, sz, "%F %T", localtime(&t));
}

/* =========================
         Send Netlink
   ========================= */
void send_diag_req(int fd, int proto) {
    struct {
        struct nlmsghdr nlh;
        struct inet_diag_req_v2 req;
    } msg;

    memset(&msg, 0, sizeof(msg));

    msg.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(msg.req));
    msg.nlh.nlmsg_type = SOCK_DIAG_BY_FAMILY;
    msg.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

    msg.req.sdiag_family = AF_INET;
    msg.req.sdiag_protocol = proto;
    msg.req.idiag_states = TCPF_ALL;

    send(fd, &msg, msg.nlh.nlmsg_len, 0);
}

/* =========================
             main
   ========================= */
void recv_diag(int nl, int ep, int proto) {
    /* --- Receive dump until done --- */
    struct epoll_event events[MAX_EVENTS];

    int timeout_ms;
    if (run_deadline_ns < 0) {
        timeout_ms = 1000;
    } else {
        long long rem = remaining_ns();
        if (rem <= 0) {
            return;
        }

        timeout_ms = (int)(rem / 1000LL);
        if (timeout_ms > 1000) {
            timeout_ms = 1000;
        }
    }

    int n = epoll_wait(ep, events, MAX_EVENTS, timeout_ms);
    if (n < 0) {
        perror("epoll_wait");
        return;
    }
    
    if (n == 0) return; // Timeout

    const char *protocol_str;
    switch (proto){
        case IPPROTO_TCP:
            protocol_str = "TCP";
            break;
        case IPPROTO_UDP:
            protocol_str = "UDP";
            break;
        default:
            protocol_str = "UNKNOWN";
    }

    for (int i = 0; i < n; i++) {
        if (events[i].data.fd != nl) continue;
        /* --- recv loop --- */
        while (1) {
            char buf[8192];

            int len = recv(nl, buf, sizeof(buf), 0);
            if (len < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return; // Nothing to read
                perror("recv");
                return;
            }

            struct nlmsghdr *h;
            for (h = (struct nlmsghdr*)buf;
                NLMSG_OK(h, len);

                h = NLMSG_NEXT(h, len)) {
                if (h->nlmsg_type == NLMSG_DONE) {
                    return; // End dump
                }

                struct inet_diag_msg *m = NLMSG_DATA(h);
                conn_key_t k;

                k.proto = proto;
                k.lip = m->id.idiag_src[0];
                k.rip = m->id.idiag_dst[0];
                k.lport = ntohs(m->id.idiag_sport);
                k.rport = ntohs(m->id.idiag_dport);

                if (is_duplicate(&k)) continue;

                char lip[64], rip[64];
                inet_ntop(AF_INET, &k.lip, lip, sizeof(lip));
                inet_ntop(AF_INET, &k.rip, rip, sizeof(rip));
                pid_t pid;
                char cmd[256] = {0};
                int found = find_pid_by_inode(m->idiag_inode, &pid, cmd, sizeof(cmd));
                char t[64];
                now(t, sizeof(t));

                
                dual_output("[%s] %s:%d/%s <---> %s:%d/%s", t, lip, k.lport, protocol_str, rip, k.rport, protocol_str);
                if (found) {
                    dual_output(" PID:%d CMD:%s", pid, cmd);
                }  
                dual_output("\n");
            }
        }
    }
}

/* =========================
             main
   ========================= */
int main(int argc, char *argv[]) {

    dual_output("SocketSpy Version 1.1\n");

    if (!(argc == 2 || argc == 3)) {
        fprintf(stderr, "Usage: %s <timeout_sec> <output_file>\n", argv[0]);
        return 1;
    }

    if (argc == 3){
        logfd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd < 0) {
            perror("open");
            return 1;
        }
    }
    
    /* --- Create netlink socket --- */
    int nl = socket(AF_NETLINK, SOCK_RAW, NETLINK_INET_DIAG);
    if (nl < 0) {
        perror("socket");
        return 1;
    }

    /* --- non-blocking --- */
    if (fcntl(nl, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl");
        return 1;
    }

    /* --- bind --- */
    struct sockaddr_nl addr = {0};
    addr.nl_family = AF_NETLINK;

    if (bind(nl, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    /* --- Create epoll --- */
    int ep = epoll_create1(0);
    if (ep < 0) {
        perror("epoll_create1");
        return 1;
    }

    /* --- Register epoll --- */
    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = nl;

    if (epoll_ctl(ep, EPOLL_CTL_ADD, nl, &ev) < 0) {
        perror("epoll_ctl");
        return 1;
    }

    /* =========================
       Main loop
       ========================= */
    int timeout_sec = atoi(argv[1]);
    if (timeout_sec > 0) {
        run_deadline_ns = monotonic_ns() + (long long)timeout_sec * 1000000000LL;
    }

    while (run_deadline_ns < 0 || remaining_ns() > 0) {

        /* --- Call to kernel (TCP) --- */
        send_diag_req(nl, IPPROTO_TCP);
        /* --- Recieve from kernel --- */
        recv_diag(nl, ep, IPPROTO_TCP);

        /* --- Call to kernel (UDP) --- */
        send_diag_req(nl, IPPROTO_UDP);
        /* --- Recieve from kernel --- */
        recv_diag(nl, ep, IPPROTO_UDP);

        if (run_deadline_ns < 0) {
            usleep(INTERVAL_MICROSEC);
        } else {
            long long rem = remaining_ns();
            if (rem <= 0) {
                break;
            }

            if (rem < (long long)INTERVAL_MICROSEC * 1000LL) {
                usleep((unsigned)rem);
            } else {
                usleep(INTERVAL_MICROSEC);
            }
        }
    }

    close(nl);
    close(ep);
    close(logfd);
    return 0;
}
