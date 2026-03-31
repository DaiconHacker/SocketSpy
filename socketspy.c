#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_CONN 4096
#define INTERVAL_USEC 1000000

typedef struct {
    char ip[64];
    int port;
} conn_t;

conn_t prev_conns[MAX_CONN];
int prev_count = 0;


void parse_addr(const char *hex, char *ip, int *port) {
    unsigned int ip_hex, port_hex;
    sscanf(hex, "%8X:%X", &ip_hex, &port_hex);

    struct in_addr addr;
    addr.s_addr = htonl(ip_hex);

    strcpy(ip, inet_ntoa(addr));
    *port = port_hex;
}


int exists(conn_t *list, int count, const char *ip, int port) {
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i].ip, ip) == 0 && list[i].port == port)
            return 1;
    }
    return 0;
}


void get_cmdline(const char *pid, char *buf, size_t size) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%s/cmdline", pid);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        strncpy(buf, "[unknown]", size);
        return;
    }

    size_t len = fread(buf, 1, size - 1, fp);
    fclose(fp);

    if (len == 0) {
        strncpy(buf, "[kernel/empty]", size);
        return;
    }

    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\0') buf[i] = ' ';
    }
    buf[len] = '\0';
}


int parse_net_file(const char *path, conn_t *out) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    char line[512];
    int count = 0;

    fgets(line, sizeof(line), fp); // skip header

    while (fgets(line, sizeof(line), fp)) {
        char local[64], remote[64];
        sscanf(line, "%*d: %63s %63s", local, remote);

        parse_addr(remote, out[count].ip, &out[count].port);
        count++;

        if (count >= MAX_CONN) break;
    }

    fclose(fp);
    return count;
}


int get_pid_conns(const char *pid, conn_t *out) {
    char path[256];
    int total = 0;

    const char *protos[] = {"tcp", "udp"};
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), "/proc/%s/net/%s", pid, protos[i]);
        total += parse_net_file(path, out + total);
    }

    return total;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <PID>\n", argv[0]);
        return 1;
    }

    char *pid = argv[1];
    char cmdline[512];

    conn_t current[MAX_CONN];

    while (1) {
        char proc_path[256];
        snprintf(proc_path, sizeof(proc_path), "/proc/%s", pid);

        if (access(proc_path, F_OK) != 0) {
            printf("PID %s exited\n", pid);
            break;
        }

        int count = get_pid_conns(pid, current);

        for (int i = 0; i < count; i++) {
            if (!exists(prev_conns, prev_count, current[i].ip, current[i].port)) {
                get_cmdline(pid, cmdline, sizeof(cmdline));
                printf("[+] PID=%s CMD='%s' -> %s:%d\n",
                       pid, cmdline, current[i].ip, current[i].port);
            }
        }

        memcpy(prev_conns, current, sizeof(conn_t) * count);
        prev_count = count;

        usleep(INTERVAL_USEC);
    }

    return 0;
}
