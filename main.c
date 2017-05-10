#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uv.h"
#include "http_parser.h"

uv_loop_t* loop = NULL;
http_parser_settings settings;
http_parser* parser = NULL;
char* reqBuf = NULL;
const int BUF_SIZE = 4096;
int bufEnd = 0;

void alloc_buffer(uv_handle_t* handle, size_t size, uv_buf_t* buf);
void connection_cb(uv_stream_t* server, int status);
void read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
void write_cb(uv_write_t* req, int status);
void close_cb(uv_handle_t* handle);
void on_header_field(http_parser*, const char *at, size_t length);
void on_header_value(http_parser*, const char *at, size_t length);

int main() {
    settings.on_header_field = (http_data_cb)on_header_field;
    settings.on_header_value = (http_data_cb)on_header_value;

    loop = uv_default_loop();
    uv_tcp_t server;
    struct sockaddr_in addr;
    uv_ip4_addr("192.168.132.136", 3333, &addr);

    uv_tcp_init(loop, &server);
    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    int r = uv_listen((uv_stream_t*)&server, 256, connection_cb);
    if (r) {
        fprintf(stdout, "Listen error: %s\n", uv_strerror(r));
        return 1;
    }
    return uv_run(loop, UV_RUN_DEFAULT);
}

void connection_cb(uv_stream_t* server, int status) {
    if (status < 0) {
        fprintf(stdout, "error in connection_cb\n");
        return;
    }
    uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, client);
    parser = malloc(sizeof(http_parser));
    http_parser_init(parser, HTTP_REQUEST);
    reqBuf = malloc(sizeof(char) * BUF_SIZE);
    memset(reqBuf, 0, sizeof(char) * BUF_SIZE);
    bufEnd = 0;
    if (uv_accept(server, (uv_stream_t*)client) == 0) {
        int r = uv_read_start((uv_stream_t*)client, alloc_buffer, read_cb);
        if (r) {
            fprintf(stdout, "Error on reading client stream: %s\n", uv_strerror(r));
            uv_close((uv_handle_t*)client, close_cb);
        }
        free(parser);
        free(reqBuf);
    } else {
        fprintf(stdout, "accept failed\n");
        free(parser);
        free(reqBuf);
        uv_close((uv_handle_t*)client, close_cb);
    }
}

void read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if (nread > 0) {
        memcpy(reqBuf + bufEnd, buf->base, nread);
        bufEnd += nread;
        free(buf->base);

        http_parser_execute(parser, &settings, reqBuf, bufEnd);
        uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
        uv_buf_t* response = malloc(sizeof(uv_buf_t));
        response->base = "HTTP/1.1 200 OK\r\nConnection:close\r\nContent-Length:11\r\n\r\nhello world\r\n\r\n";
        response->len = strlen(response->base);
        int r = uv_write(req, stream, response, 1, write_cb);
        if (r) {
            fprintf(stdout, "Error on writing client stream: %s\n", uv_strerror(r));
            uv_close((uv_handle_t*)stream, close_cb);
        }
        free(response);
    } else if (nread == UV_EOF) {
        uv_close((uv_handle_t*)stream, close_cb);
    } else if (nread < 0) {
        fprintf(stdout, "error in read_cb: %s\n", uv_strerror(nread));
        uv_close((uv_handle_t*)stream, close_cb);
    }
}

void close_cb(uv_handle_t* handle) {
    free(handle);
}

void write_cb(uv_write_t* req, int status) {
    if (status < 0) {
        fprintf(stdout, "error in write_cb:%s\n", uv_strerror(status));
        return;
    }
    uv_close(req->handle, close_cb);
    free(req);
}

void alloc_buffer(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
    *buf = uv_buf_init((char*)malloc(size), size);
}

void on_header_field(http_parser* parser, const char *at, size_t length) {
    char* name = malloc(sizeof(length)*length + 1);
    memset(name, 0, sizeof(length)*length + 1);
    memcpy(name, at, length);
    // fprintf(stdout, "header name: %s\n", name);
    free(name);
}

void on_header_value(http_parser* parser, const char *at, size_t length) {
    char* value = malloc(sizeof(length)*length + 1);
    memset(value, 0, sizeof(length)*length + 1);
    memcpy(value, at, length);
    // fprintf(stdout, "header value: %s\n", value);
    free(value);
}
