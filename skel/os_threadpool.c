// SPDX-License-Identifier: BSD-3-Clause

#include "os_threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* === TASK === */

/* Creates a task that thread must execute */
os_task_t *task_create(void *arg, void (*f)(void *))
{
	os_task_t *task = (os_task_t *)malloc(sizeof(os_task_t));

	task->task = f;
	task->argument = arg;
	return task;
}

/* Add a new task to threadpool task queue */
void add_task_in_queue(os_threadpool_t *tp, os_task_t *t)
{
	os_task_queue_t *new_node = malloc(sizeof(os_task_queue_t));

	if (new_node == NULL) {
		printf("new node malloc failed\n");
		return;
	}

	new_node->next = NULL;
	new_node->task = t;

	pthread_mutex_lock(&tp->taskLock);

	os_task_queue_t *current = tp->tasks;

	// If task queue is empty
	if (!current) {
		tp->tasks = new_node;
		pthread_mutex_unlock(&tp->taskLock);
		return;
	}

	// Add task to the queue
	while (current && current->next)
		current = current->next;

	if (!current)
		tp->tasks = new_node;
	else
		current->next = new_node;

	pthread_mutex_unlock(&tp->taskLock);
}

/* Get the head of task queue from threadpool */
os_task_t *get_task(os_threadpool_t *tp)
{
	pthread_mutex_lock(&tp->taskLock);

	os_task_queue_t *task_queue_head = tp->tasks;

	if (!task_queue_head) {
		pthread_mutex_unlock(&tp->taskLock);
		return NULL;
	}

	os_task_t *task = task_queue_head->task;

	tp->tasks = task_queue_head->next;

	free(task_queue_head);
	task_queue_head = NULL;

	pthread_mutex_unlock(&tp->taskLock);

	return task;
}

/* === THREAD POOL === */

/* Initialize the new threadpool */
os_threadpool_t *threadpool_create(unsigned int nTasks, unsigned int nThreads)
{
	os_threadpool_t *tp = (os_threadpool_t *)malloc(sizeof(os_threadpool_t));

	if (tp == NULL) {
		printf("new threadpool malloc failed\n");
		return NULL;
	}

	tp->should_stop = 0;
	tp->num_threads = nThreads;

	pthread_mutex_init(&tp->taskLock, NULL);

	tp->threads = (pthread_t *)malloc(nThreads * sizeof(pthread_t));
	if (tp->threads == NULL) {
		printf("thread array malloc failed\n");
		return NULL;
	}

	for (int i = 0; i < nThreads; ++i) {
		pthread_t current_thread = tp->threads[i];
		int ret = pthread_create(&current_thread, NULL, thread_loop_function, (void *)tp);

		if (ret) {
			printf("failed new thread create\n");
			return NULL;
		}
	}

	return tp;
}

/* Loop function for threads */
void *thread_loop_function(void *args)
{
	os_threadpool_t *tp = (os_threadpool_t *)args;
	os_task_t *current_task;

	while (1) {
		// Pop task queue head
		current_task = get_task(tp);

		if (current_task && current_task->task) {
			// Run task function
			current_task->task(current_task->argument);
			free(current_task);
			current_task = NULL;
		}

		if (tp->should_stop)
			break;
	}

	return NULL;
}

/* Stop the thread pool once a condition is met */
void threadpool_stop(os_threadpool_t *tp, int (*processing_done)(os_threadpool_t *))
{
	// Check if the whole graph is processed
	if (processing_done(tp)) {
		tp->should_stop = 1;

		// Join all threads
		for (int i = 0; i < tp->num_threads; i++)
			pthread_join(tp->threads[i], NULL);
	}
}
