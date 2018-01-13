#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "uv.h"
#include "http_parser.h"

uv_loop_t *loop;
uv_pipe_t queue;

char* reqBuf;
const int BUF_SIZE = 4096;
int bufEnd = 0;

void allocBuffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
	*buf = uv_buf_init((char*)malloc(suggested_size), suggested_size);
}

void close_cb(uv_handle_t* handle) {
	free(handle);
}

void write_cb(uv_write_t* req, int status) {
	uv_close((uv_handle_t*)req->handle, close_cb);
	free(req);
}

void echoRead(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
	fprintf(stderr, "nread: %d\n", nread);
	if (nread == UV_EOF) {
		uv_close((uv_handle_t*)stream, close_cb);
		return;
	}

	if (nread < 0) {
		fprintf(stderr, "Read error: %s\n", uv_err_name(nread));
		uv_close((uv_handle_t*)stream, close_cb);
		return;
	}
	
	if (reqBuf == NULL) {
		reqBuf = calloc(BUF_SIZE, sizeof(char));
		bufEnd = 0;
	}
	memcpy(reqBuf + bufEnd, buf->base, nread);
	bufEnd += nread;
	free(buf->base);
	fprintf(stderr, "request: %s", reqBuf);
	free(reqBuf);
	reqBuf = NULL;
	uv_write_t* req = calloc(1, sizeof(uv_write_t));
	uv_buf_t* response = calloc(1, sizeof(uv_buf_t));
	response->base = "http/1.1 200 OK\r\nConnection:close\r\nContent-Length:11\r\n\r\nhello wolrd\r\n\r\n";
	response->len = strlen(response->base);
	int r = uv_write(req, stream, response, 1, write_cb);
}

void onNewConnection(uv_stream_t *q, ssize_t nread, const uv_buf_t *buf) {
	if (nread < 0) {
		fprintf(stderr, "Read error %s\n", uv_err_name(nread));
		uv_close((uv_handle_t*)q, NULL);
		return;
	}

	uv_pipe_t *pipe = (uv_pipe_t*)q;
	if (!uv_pipe_pending_count(pipe)) {
		fprintf(stderr, "No pending count\n");
		return;
	}

	uv_handle_type pending = uv_pipe_pending_type(pipe);
	assert(pending == UV_TCP);

	uv_tcp_t *client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
	uv_tcp_init(loop, client);
	if (uv_accept(q, (uv_stream_t*)client) == 0) {
		uv_os_fd_t fd;
		uv_fileno((const uv_handle_t*)client, &fd);
		fprintf(stderr, "Worker %d: Accepted fd %d\n", getpid(), fd);
		uv_read_start((uv_stream_t*)client, allocBuffer, echoRead);
	} else {
		uv_close((uv_handle_t*)client, NULL);
	}
}

int main() {
	loop = uv_default_loop();

	uv_pipe_init(loop, &queue, 1);
	uv_pipe_open(&queue, 0);
	uv_read_start((uv_stream_t*)&queue, allocBuffer, onNewConnection);
	return uv_run(loop, UV_RUN_DEFAULT);
}
