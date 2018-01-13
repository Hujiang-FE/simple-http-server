#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uv.h"

struct childWorker {
	uv_process_t req;
	uv_process_options_t options;
	uv_pipe_t pipe;
} *workers;

int roundRobinCounter = 0;
int childWorkerCount = 0;
uv_loop_t* loop = NULL;

void onNewConnection(uv_stream_t *server, int status) {
	if (status == -1) {
		return;
	}

	uv_tcp_t *client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
	uv_tcp_init(loop, client);
	if (uv_accept(server, (uv_stream_t*)client) == 0) {
		uv_write_t *writeReq = (uv_write_t*) malloc(sizeof(uv_write_t));
		uv_buf_t dummy_buf = uv_buf_init("a", 1);
		struct childWorker *worker = &workers[roundRobinCounter];
		uv_write2(writeReq, (uv_stream_t*)&worker->pipe, &dummy_buf, 1, (uv_stream_t*)client, NULL);
		roundRobinCounter = (roundRobinCounter + 1) % childWorkerCount;
	} else {
		uv_close((uv_handle_t*)client, NULL);
	}
}

void close_process_handle(uv_process_t* worker, int64_t exitStatus, int termSignal) {
}

void setupWorkers(char* args[]) {
	roundRobinCounter = 0;
	uv_cpu_info_t *info;
	int cpuCount;
	uv_cpu_info(&info, &cpuCount);
	uv_free_cpu_info(info, cpuCount);

	childWorkerCount = cpuCount;
	
	workers = calloc(cpuCount, sizeof(struct childWorker));
	while (cpuCount--) {
		struct childWorker *worker = &workers[cpuCount];
		uv_pipe_init(loop, &worker->pipe, 1);
		uv_stdio_container_t* childStdio = calloc(3, sizeof(uv_stdio_container_t));
		childStdio[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
		childStdio[0].data.stream = (uv_stream_t*)&worker->pipe;
		childStdio[1].flags = UV_IGNORE;
		childStdio[2].flags = UV_INHERIT_FD;
		childStdio[2].data.fd = 2;

		worker->options.stdio = childStdio;
		worker->options.stdio_count = 3;

		worker->options.exit_cb = close_process_handle;
		worker->options.file = args[1];
		// worker->options.args = args;

		uv_spawn(loop, &worker->req, &worker->options);
		printf("Started worker %d\n", worker->req.pid);
	}
}

int main(int argc, char* argv[]) {
	loop = uv_default_loop();
	setupWorkers(argv);
	
	uv_tcp_t server;
	struct sockaddr_in addr;
	uv_ip4_addr("127.0.0.1", 3333, &addr);

	uv_tcp_init(loop, &server);
	uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
	int r = uv_listen((uv_stream_t*)&server, 256, onNewConnection);
	if (r) {
		fprintf(stdout, "Listen error: %s\n", uv_strerror(r));
		return 1;
	}
	
	return uv_run(loop, UV_RUN_DEFAULT);
}
