#ifndef QUEUE
#define QUEUE

// https://gist.github.com/rdleon/d569a219c6144c4dfc04366fd6298554
struct Queue {
	int size;
	int max_size;
	struct Node *head;
	struct Node *tail;
};

extern struct Queue *create_queue(int capacity);
extern int enqueue(struct Queue *q, void *value);
extern void *dequeue(struct Queue *q);
extern void free_queue(struct Queue *q);

#endif
