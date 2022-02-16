#include <stdlib.h>
#include <stdio.h>
#include "queue.h"

struct Node {
	void *value;
	struct Node *next;
};

struct Queue *create_queue(int capacity) {
	struct Queue *q;
	q = malloc(sizeof(struct Queue));

	if (q == NULL)
		return q;

	q->size = 0;
	q->max_size = capacity;
	q->head = NULL;
	q->tail = NULL;

	return q;
}

int enqueue(struct Queue *q, void *value)
{
	if ((q->size + 1) > q->max_size)
		return 0;

	struct Node *node = malloc(sizeof(struct Node));

	if (node == NULL)
		return 0;

	node->value = value;
	node->next = NULL;

	if (q->head == NULL) {
		q->head = node;
		q->tail = node;
		q->size = 1;

		return 1;
	}

	q->tail->next = node;
	q->tail = node;
	q->size += 1;

	return 1;
}

void* dequeue(struct Queue *q)
{
	if (q->size == 0)
		return NULL;

	void *value = NULL;
	struct Node *tmp = NULL;

	value = q->head->value;
	tmp = q->head;
	q->head = q->head->next;
	q->size -= 1;

	free(tmp);

	return value;
}

void free_queue(struct Queue *q)
{
	if (q == NULL)
		return;

	while (q->head != NULL) {
		struct Node *tmp = q->head;
		q->head = q->head->next;

		if (tmp->value != NULL)
			free(tmp->value);

		free(tmp);
	}

	free (q);
}
