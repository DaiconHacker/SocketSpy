// gcc -Wall -O2 -static ./socketspy.c -o socketspy

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

#define MAX_EVENTS 4
#define HASH_SIZE 65536
#define TCPF_ALL 0xFFF
#define INTERVAL_MICROSEC 1000

/* =========================
  Connection key (for deduplication)
   ========================= */
typedef struct {
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
    return (k->lip ^ k->rip ^ k->lport ^ k->rport) % HASH_SIZE;
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
void send_diag_req(int fd) {
    struct {
        struct nlmsghdr nlh;
        struct inet_diag_req_v2 req;
    } msg;

    memset(&msg, 0, sizeof(msg));

    msg.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(msg.req));
    msg.nlh.nlmsg_type = SOCK_DIAG_BY_FAMILY;
    msg.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

    msg.req.sdiag_family = AF_INET;
    msg.req.sdiag_protocol = IPPROTO_TCP;
    msg.req.idiag_states = TCPF_ALL;

    send(fd, &msg, msg.nlh.nlmsg_len, 0);
}

/* =========================
             main
   ========================= */
int main() {
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
    while (1) {

        /* --- Call to kernel --- */
        send_diag_req(nl);

        int done = 0;

        /* --- Receive dump until done --- */
        while (!done) {

            struct epoll_event events[MAX_EVENTS];

            int n = epoll_wait(ep, events, MAX_EVENTS, 1000);
            if (n < 0) {
                perror("epoll_wait");
                break;
            }

            if (n == 0) continue; // Timeout

            for (int i = 0; i < n; i++) {

                if (events[i].data.fd != nl) continue;

                /* --- recv loop --- */
                while (1) {

                    char buf[8192];
                    int len = recv(nl, buf, sizeof(buf), 0);

                    if (len < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break; // Nothing to read
                        perror("recv");
                        done = 1;
                        break;
                    }

                    struct nlmsghdr *h;

                    for (h = (struct nlmsghdr*)buf;
                         NLMSG_OK(h, len);
                         h = NLMSG_NEXT(h, len)) {

                        if (h->nlmsg_type == NLMSG_DONE) {
                            done = 1; // End dump
                            break;
                        }

                        struct inet_diag_msg *m = NLMSG_DATA(h);

                        conn_key_t k;
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

                        int found = find_pid_by_inode(
                            m->idiag_inode, &pid, cmd, sizeof(cmd));

                        char t[64];
                        now(t, sizeof(t));

                        printf("[%s] %s:%d <---> %s:%d",
                               t, lip, k.lport, rip, k.rport);

                        if (found)
                            printf(" PID:%d CMD:%s", pid, cmd);

                        printf("\n");
                    }
                }
            }
        }

        /* --- Wait for next polling --- */
        usleep(INTERVAL_MICROSEC);
    }

    close(nl);
    close(ep);
    return 0;
}
