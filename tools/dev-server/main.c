/*
 * Forge Dev Server
 *
 * A minimal HTTP server + file watcher for local development.
 *
 * Features:
 *   - Serves static files from a directory
 *   - Watches .cx files for changes and re-compiles them
 *   - Sends Server-Sent Events (SSE) to browser for hot reload
 *   - Runs on port 3000 by default
 *
 * Usage:
 *   forge dev [--port 3000] [--dir ./]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>

#define DEV_DEFAULT_PORT   3000
#define DEV_BACKLOG        8
#define DEV_BUF_SIZE       8192
#define DEV_MAX_WATCH      128

static volatile int _running = 1;
static int          _port    = DEV_DEFAULT_PORT;
static char         _dir[512] = "./";
static char         _forge_bin[512] = "./build/forge";

/* ─── MIME Types ──────────────────────────────────────────────────────────── */

static const char *mime_for(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".js")   == 0) return "application/javascript";
    if (strcmp(ext, ".css")  == 0) return "text/css";
    if (strcmp(ext, ".wasm") == 0) return "application/wasm";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".svg")  == 0) return "image/svg+xml";
    if (strcmp(ext, ".png")  == 0) return "image/png";
    if (strcmp(ext, ".ico")  == 0) return "image/x-icon";
    return "application/octet-stream";
}

/* ─── HTTP Response Helpers ───────────────────────────────────────────────── */

static void send_response(int fd, int status, const char *mime,
                           const char *body, size_t body_len) {
    char header[1024];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, mime, body_len);
    write(fd, header, (size_t)hlen);
    if (body && body_len > 0) write(fd, body, body_len);
}

static void send_404(int fd) {
    const char *body = "<h1>404 Not Found</h1>";
    send_response(fd, 404, "text/html", body, strlen(body));
}

static void send_file(int fd, const char *path) {
    int f = open(path, O_RDONLY);
    if (f < 0) { send_404(fd); return; }

    struct stat st;
    fstat(f, &st);
    size_t size = (size_t)st.st_size;

    char *buf = malloc(size);
    if (!buf) { close(f); send_404(fd); return; }
    read(f, buf, size);
    close(f);

    send_response(fd, 200, mime_for(path), buf, size);
    free(buf);
}

/* ─── Hot Reload SSE Endpoint ─────────────────────────────────────────────── */

static int _sse_clients[64];
static int _sse_count = 0;
static pthread_mutex_t _sse_mutex = PTHREAD_MUTEX_INITIALIZER;

static void send_sse_reload(void) {
    const char *msg = "data: reload\n\n";
    pthread_mutex_lock(&_sse_mutex);
    for (int i = 0; i < _sse_count; i++) {
        write(_sse_clients[i], msg, strlen(msg));
    }
    pthread_mutex_unlock(&_sse_mutex);
}

static void handle_sse(int fd) {
    const char *headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        ": connected\n\n";
    write(fd, headers, strlen(headers));

    pthread_mutex_lock(&_sse_mutex);
    if (_sse_count < 64) _sse_clients[_sse_count++] = fd;
    pthread_mutex_unlock(&_sse_mutex);
    /* Client fd is held open — closed by watcher thread on exit */
}

/* ─── File Watcher Thread ─────────────────────────────────────────────────── */

typedef struct {
    char   path[512];
    time_t mtime;
} WatchEntry;

static WatchEntry _watched[DEV_MAX_WATCH];
static int        _watch_count = 0;

static void watch_add(const char *path) {
    if (_watch_count >= DEV_MAX_WATCH) return;
    struct stat st;
    if (stat(path, &st) != 0) return;
    strncpy(_watched[_watch_count].path, path, 511);
    _watched[_watch_count].mtime = st.st_mtime;
    _watch_count++;
}

static void *watcher_thread(void *arg) {
    (void)arg;
    printf("forge: watching %d files for changes...\n", _watch_count);

    while (_running) {
        sleep(1);
        for (int i = 0; i < _watch_count; i++) {
            struct stat st;
            if (stat(_watched[i].path, &st) != 0) continue;
            if (st.st_mtime != _watched[i].mtime) {
                _watched[i].mtime = st.st_mtime;
                printf("forge: \033[33m changed:\033[0m %s — recompiling...\n",
                       _watched[i].path);

                /* Re-compile the changed file */
                char cmd[1024];
                snprintf(cmd, sizeof(cmd),
                         "%s compile --no-wasm -o dist %s",
                         _forge_bin, _watched[i].path);
                int rc = system(cmd);
                if (rc == 0) {
                    printf("forge: \033[32m rebuilt\033[0m  — notifying browser\n");
                    send_sse_reload();
                } else {
                    printf("forge: \033[31m build failed\033[0m\n");
                }
            }
        }
    }
    return NULL;
}

/* ─── Request Handler ─────────────────────────────────────────────────────── */

static void handle_request(int fd) {
    char buf[DEV_BUF_SIZE];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) { close(fd); return; }
    buf[n] = '\0';

    /* Parse method and path */
    char method[8], path[512];
    sscanf(buf, "%7s %511s", method, path);

    if (strcmp(method, "GET") != 0) { send_404(fd); close(fd); return; }

    /* SSE endpoint for hot reload */
    if (strcmp(path, "/__forge_sse") == 0) {
        handle_sse(fd); /* fd kept open */
        return;
    }

    /* Root → index.html */
    if (strcmp(path, "/") == 0) strcpy(path, "/index.html");

    /* Resolve to filesystem path */
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%s", _dir, path + 1);

    /* Security: prevent path traversal */
    if (strstr(full_path, "..")) { send_404(fd); close(fd); return; }

    send_file(fd, full_path);
    close(fd);
}

/* ─── Main Server Loop ────────────────────────────────────────────────────── */

static void on_signal(int sig) { (void)sig; _running = 0; }

int main(int argc, char **argv) {
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            _port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            strncpy(_dir, argv[++i], sizeof(_dir) - 2);
            /* ensure trailing slash */
            size_t l = strlen(_dir);
            if (_dir[l-1] != '/') { _dir[l] = '/'; _dir[l+1] = '\0'; }
        } else if (strcmp(argv[i], "--forge") == 0 && i + 1 < argc) {
            strncpy(_forge_bin, argv[++i], sizeof(_forge_bin) - 1);
        }
    }

    /* Watch all .cx files in dir */
    char find_cmd[512];
    snprintf(find_cmd, sizeof(find_cmd), "find %s -name '*.cx' 2>/dev/null", _dir);
    FILE *fp = popen(find_cmd, "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = '\0';
            watch_add(line);
        }
        pclose(fp);
    }

    /* Start watcher thread */
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
    pthread_t wt;
    pthread_create(&wt, NULL, watcher_thread, NULL);

    /* Create server socket */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "forge dev: cannot bind port %d\n", _port);
        return 1;
    }
    listen(srv, DEV_BACKLOG);

    printf("\n\033[32m  Forge Dev Server\033[0m  v0.1.0\n");
    printf("  \033[36mLocal:\033[0m   http://localhost:%d\n", _port);
    printf("  \033[36mServing:\033[0m %s\n\n", _dir);
    printf("  Press Ctrl+C to stop\n\n");

    while (_running) {
        int client = accept(srv, NULL, NULL);
        if (client < 0) continue;
        handle_request(client);
    }

    close(srv);
    pthread_join(wt, NULL);
    printf("\nforge dev: stopped.\n");
    return 0;
}
