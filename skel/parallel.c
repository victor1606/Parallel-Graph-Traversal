// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "os_list.h"

#define MAX_TASK 100
#define MAX_THREAD 4

os_graph_t *graph;
os_threadpool_t *tp;

pthread_mutex_t process_node_mutex;
pthread_mutex_t inc_mutex;

int sum;
int processed_nodes;

// Checks if all nodes have been visited
int processing_done(os_threadpool_t *tp)
{
	return processed_nodes == graph->nCount;
}

// Thread task function
void processNode(int index)
{
	os_node_t *node = graph->nodes[index];

	pthread_mutex_lock(&inc_mutex);

	sum += node->nodeInfo;
	processed_nodes++;

	pthread_mutex_unlock(&inc_mutex);

	for (int i = 0; i < node->cNeighbours; ++i) {
		pthread_mutex_lock(&process_node_mutex);

		// Check if node has been processed
		if (!graph->visited[node->neighbours[i]]) {
			add_task_in_queue(tp, task_create((void *)(size_t)node->neighbours[i], (void (*)(void *))processNode));
			graph->visited[node->neighbours[i]]++;
		}

		pthread_mutex_unlock(&process_node_mutex);
	}

	// Check if processing is done
	threadpool_stop(tp, processing_done);
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: ./%s input_file\n", __func__);
		exit(1);
	}

	FILE *input_file = fopen(argv[1], "r");

	if (input_file == NULL) {
		printf("[Error] Can't open file\n");
		return -1;
	}

	graph = create_graph_from_file(input_file);

	if (graph == NULL) {
		printf("[Error] Can't read the graph from file\n");
		return -1;
	}

	tp = threadpool_create(graph->nCount, MAX_THREAD);

	pthread_mutex_init(&inc_mutex, NULL);
	pthread_mutex_init(&process_node_mutex, NULL);

	for (int i = 0; i < graph->nCount; ++i) {
		pthread_mutex_lock(&process_node_mutex);

		// Check if node has been processed
		if (!graph->visited[i]) {
			// Add processNode tasks to the queue
			add_task_in_queue(tp, task_create((void *)(size_t)i, (void (*)(void *))processNode));
			graph->visited[i]++;
		}
		pthread_mutex_unlock(&process_node_mutex);
	}

	// Make threads wait until the whole graph is processed
	while (!processing_done(tp))
		continue;

	// Stop all threads
	threadpool_stop(tp, processing_done);

	free(tp->threads);
	free(tp->tasks);
	free(tp);

	pthread_mutex_destroy(&inc_mutex);
	pthread_mutex_destroy(&process_node_mutex);

	printf("%d", sum);
	return 0;
}

