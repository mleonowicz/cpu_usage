#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "queue.h"

typedef struct {
    unsigned long t_user;
    unsigned long t_nice;
    unsigned long t_system;
    unsigned long t_idle;
    unsigned long t_iowait;
    unsigned long t_irq;
    unsigned long t_softirq;
    unsigned long t_steal;
    unsigned long t_guest;
    unsigned long t_guest_nice;

} cpustat;

typedef struct {
    struct Queue *queue;
    sem_t semaphore;
} producent_consumer;

typedef struct {
    sem_t semaphore;
    int is_active;
} watchdog_wrapper;

producent_consumer reader_analyzer;
producent_consumer analyzer_printer;

watchdog_wrapper watchdog_reader;
watchdog_wrapper watchdog_analyzer;
watchdog_wrapper watchdog_printer;

volatile int done = 0;
long num_of_cpu = -1;

void sig_handler(int signum) {
    if (signum == SIGTERM)
        done = 1;
}

void *reader() {
    static size_t buffer_size = 128;
    size_t offset = 0;
    size_t length = 0;
    size_t size_of_file = 0;
    char *buffer = NULL;

    while (!done) {
        FILE *file = fopen("/proc/stat", "r");
        offset = 0;

        // read the file with unknown size
        buffer = malloc(sizeof(char) * buffer_size);
        while ((length = fread(buffer + offset, 1, buffer_size - offset, file)) == (buffer_size - offset)) {
            size_of_file += length;
            offset = buffer_size;
            buffer_size *= 2;
            buffer = realloc(buffer, buffer_size * sizeof(char));
        }

        // fread doesnt do that for us
        buffer[size_of_file] = '\0';

        // enqueue our data
        sem_wait(&reader_analyzer.semaphore);
        int result = enqueue(reader_analyzer.queue, buffer);
        sem_post(&reader_analyzer.semaphore);

        if (result == 0)
            free(buffer);

        buffer = NULL;
        fclose(file);

        // inform our watchdog that we're still working
        sem_wait(&watchdog_reader.semaphore);
        watchdog_reader.is_active = 1;
        sem_post(&watchdog_reader.semaphore);

        sleep(1);
    }

    free(buffer);
    return NULL;
}

void *analyzer() {
    cpustat *prev = malloc(sizeof(cpustat) * num_of_cpu);
    int first_read = 1;
    char *data = NULL;

    while (!done) {
        sem_wait(&reader_analyzer.semaphore);
        data = dequeue(reader_analyzer.queue);
        sem_post(&reader_analyzer.semaphore);

        if (data != NULL) {
            double *results = malloc(sizeof(double) * num_of_cpu);

            char *token  = strtok(data, "cpu");
            int index = -1;

            while (token != NULL) {
                char cpu[255];

                if (index >= 0 && index < num_of_cpu) {
                    cpustat cur;

                    // parse the read file
                    sscanf(token, "%s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
                            cpu, &cur.t_user, &cur.t_nice, &cur.t_system, &cur.t_idle, &cur.t_iowait, 
                            &cur.t_irq, &cur.t_softirq, &cur.t_steal, &cur.t_guest, &cur.t_guest_nice);

                    // compute the cpu usage
                    // https://www.kgoettler.com/post/proc-stat/
                    if (!first_read) {
                        int idle_prev = (prev[index].t_idle) + (prev[index].t_iowait);
                        int idle_cur = (cur.t_idle) + (cur.t_iowait);

                        int nidle_prev = (prev[index].t_user) + (prev[index].t_nice) + (prev[index].t_system) + (prev[index].t_irq) + (prev[index].t_softirq);
                        int nidle_cur = (cur.t_user) + (cur.t_nice) + (cur.t_system) + (cur.t_irq) + (cur.t_softirq);

                        int total_prev = idle_prev + nidle_prev;
                        int total_cur = idle_cur + nidle_cur;

                        double totald = (double) total_cur - (double) total_prev;
                        double idled = (double) idle_cur - (double) idle_prev;

                        double cpu_perc = (1000 * (totald - idled) / totald + 1) / 10;
                        results[index] = cpu_perc;
                    }

                    memcpy(prev + index, &cur, sizeof(cpustat));
                }

                index++;
                token = strtok(NULL, "cpu");
            }
            
            // we need this flag because two calculate cpu usage we need
            // a current and a previous read
            if (!first_read) {
                sem_wait(&analyzer_printer.semaphore);
                int result = enqueue(analyzer_printer.queue, results);
                sem_post(&analyzer_printer.semaphore);

                if (result == 0)
                    free(results);
                results = NULL;
            }
            else
                free(results);

            first_read = 0;
            free(data);
            data = NULL;

            // inform our watchdog that we're still working
            sem_wait(&watchdog_analyzer.semaphore);
            watchdog_analyzer.is_active = 1;
            sem_post(&watchdog_analyzer.semaphore);
        }

        sleep(1);
    }

    free(data);
    free(prev);
    return NULL;
}

void *printer() {

    while (!done) {
        sem_wait(&analyzer_printer.semaphore);
        double *data = dequeue(analyzer_printer.queue);
        sem_post(&analyzer_printer.semaphore);

        if (data != NULL) {
            for (int i = 0; i < num_of_cpu; i++)
                printf("CPU%d: %.2f %%\n", i, data[i]);
            printf("\n");

            free(data);
            data = NULL;

            // inform our watchdog that we're still working
            sem_wait(&watchdog_printer.semaphore);
            watchdog_printer.is_active = 1;
            sem_post(&watchdog_printer.semaphore);
        }

        sleep(1);
    }

    return NULL;
}

void *watchdog() {
    sleep(2);

    while (!done)
    {
        sem_wait(&watchdog_reader.semaphore);
        if (!watchdog_reader.is_active) {
            done = 1;
            printf("Reader doesn't answer!");
            return NULL;
        }
        watchdog_reader.is_active = 0;
        sem_post(&watchdog_reader.semaphore);

        sem_wait(&watchdog_analyzer.semaphore);
        if (!watchdog_analyzer.is_active) {
            done = 1;
            printf("Analyzer doesn't answer!");
            return NULL;
        }
        watchdog_analyzer.is_active = 0;
        sem_post(&watchdog_analyzer.semaphore);

        sem_wait(&watchdog_printer.semaphore);
        if (!watchdog_printer.is_active) {
            done = 1;
            printf("Printer doesn't answer!");
            return NULL;
        }
        watchdog_printer.is_active = 0;
        sem_post(&watchdog_printer.semaphore);

        sleep(2);
    }
    return NULL;
}

int main() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigaction(SIGTERM, &sa, NULL);

    num_of_cpu = sysconf(_SC_NPROCESSORS_ONLN);

    sem_init(&reader_analyzer.semaphore, 0, 1);
    reader_analyzer.queue = create_queue(5);
    sem_init(&analyzer_printer.semaphore, 0, 1);
    analyzer_printer.queue = create_queue(5);

    watchdog_reader.is_active = 1;
    sem_init(&watchdog_reader.semaphore, 0, 1);
    watchdog_analyzer.is_active = 1;
    sem_init(&watchdog_analyzer.semaphore, 0, 1);
    watchdog_printer.is_active = 1;
    sem_init(&watchdog_printer.semaphore, 0, 1);

    pthread_t reader_thread, analyzer_thread, printer_thread, watchdog_thread;

    printf("Gathering information...\n");

    pthread_create(&reader_thread, NULL, reader, NULL);
    pthread_create(&analyzer_thread, NULL, analyzer, NULL);
    pthread_create(&printer_thread, NULL, printer, NULL);
    pthread_create(&watchdog_thread, NULL, watchdog, NULL);

    pthread_join(reader_thread, NULL);
    pthread_join(analyzer_thread, NULL);
    pthread_join(printer_thread, NULL);
    pthread_join(watchdog_thread, NULL);

    free_queue(reader_analyzer.queue);
    free_queue(analyzer_printer.queue);

    sem_destroy(&reader_analyzer.semaphore);
    sem_destroy(&analyzer_printer.semaphore);

    return 0;
}
