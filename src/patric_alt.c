/**
 * @file patric_handout.c
 * @author Emely Birkhofen, Finn Evers, Merlin Felix (cauelite@cauelite.com)
 * @brief Ist da die Kroße Krabbe?
 * @version 0.1
 * @date 2022-11-28
 * 
 * @copyright Copyright (c) 2022 Emely Birkhofen, Finn Evers, Merlin Felix
 * 
 */

#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>
#include <limits.h>

#include <ctype.h>
#include <string.h>

#include "triangle.h"

#ifdef DEBUG
    #define PRINTER_MSG\
        "Found %d boundary and %d interior points, "\
        "%d active threads, %d finished threads\n"
#else
    #define PRINTER_MSG\
        "\rFound %d boundary and %d interior points, "\
        "%d active threads, %d finished threads"
#endif

/* The shared state in our concurrent scenario */
struct state {
  int boundary, interior;

  int active, finished;
} state = {};

/* semaphores for the output thread, the worker dispatch thread, and access to the state */
sem_t sem_output_wait, sem_worker_dispatch, sem_state_lock;

/**
 * @brief Callback function for the countPoints() function of triangle.h.
 * This function should increment the number of found points by the given amount and signal the
 * output thread to update the progress report on stdout.
 * @param boundary Found points on the boundary of the triangle
 * @param interior Found points in the interior of the triangle
 */
static void calc_finished_cb(int boundary, int interior) {
    sem_wait(&sem_state_lock);
    state.boundary += boundary;
    state.interior += interior;
    sem_post(&sem_state_lock);
}

/**
 * @brief Start routine of the thread that is meant to present the results.
 * @param param the param of our thread
 */
static void *printer(void *param)
{
    for (;;) {
        /* Await print job */
        sem_wait(&sem_output_wait);

        /* Claim safe access on the state */
        sem_wait(&sem_state_lock);

        /* copy the state */
        struct state statcpy = {
            .boundary   = state.boundary,
            .interior   = state.interior,
            .active     = state.active,
            .finished   = state.finished
        };
        
        /* Grant access on the state */
        sem_post(&sem_state_lock);
        
        /* Print job */
        printf(
            PRINTER_MSG,
            statcpy.boundary,
            statcpy.interior,
            statcpy.active,
            statcpy.finished
        );
        fflush(stdout);
    }

    return NULL;
}

/* - joa points und trianlges halt - */

/**
 * @brief Ein 2D Punkt
 * Binär-kompatibel zu struct coordinate in triangle.h
 */
struct Point {
    int x;
    int y;
};

/**
 * @brief Prints a triangle to stdout.
 * 
 * @param points A triangle represented by a points-array
 * @return printf's return value
 */
int print_triangle(const struct Point *points);

/* - input parsing - */

/**
 * @brief Contains all information about an occured parse-error.
 */
struct ParseError {
    char *ln;
    char *from, *to;
    const char *message;
};

/**
 * @brief Prints a parse error to stderr.
 * 
 * @param parse_error Information about the occured error. 
 */
void parse_error_print(struct ParseError *parse_error);

/**
 * @brief Parses an integer.
 * 
 * @param str The string to parse from.
 * @param out_i A pointer to the resulting integer.
 * @param out_err A pointer to where information about
 *                  a possible error should be stored.
 * @param allow_neg Where or not to accept negative integers.
 * @return A pointer to the next char after the parsed integer;
 *         NULL if a parsing error occured.
 */
char *parse_int(char *str, int *out_i, struct ParseError *out_error, bool allow_neg);

/**
 * @brief Parses a point.
 * 
 * @param str The string to parse from.
 * @param out_point A pointer to the resulting point.
 * @param out_error A pointer to where information
 *                  about a possible error should be stored.
 * @return A pointer to the next char after the parsed point;
 *         NULL if a parsing error occured.
 */
char *parse_point(char *str, struct Point *out_point, struct ParseError *out_error);

/**
 * @brief Parses a triangle.
 * 
 * @param str The string to parse from.
 * @param out_points A pointer to the resulting triangle.
 * @param out_error A pointer to where information
 *                  about a possible error should be stored.
 * @return A pointer to the next char after the parsed triangle;
 *         NULL if a parsing error occured.
 */
char *parse_triangle(char *str, struct Point *out_points, struct ParseError *out_error);

/**
 * @brief Reads and parses the next input.
 * 
 * @param buf A buffer to read the intput into.
 * @param last A pointer to a pointer to the next input
 *             (needed if multiple triangles were read, separated by a ';'.)
 * @param bufsz The size of the buffer 'buf'.
 * @param out_points A pointer to a point-array into which the result should be parsed.
 * @return true if successful; false otherwise
 */
bool read_next_input(char *buf, char **last, size_t bufsz, struct Point *out_points);

/* - queue - */

/**
 * @brief An Node of a triangle queue.
 */
struct QueueElem;

/**
 * @brief A thread-safe queue that waits passivly on dequeue if empty.
 */
struct Queue {
    struct QueueElem *head;
    struct QueueElem *end;
    sem_t sem_elements;
    sem_t lock;
};

/**
 * @brief Initializes a thread-safe trianlge ueueu
 * 
 * @param queue The queue to initialize
 * @return true on success, false otherwise
 */
bool queue_init(struct Queue *queue);

/**
 * @brief Thread-safe enqueues a triangle into a queue.
 * 
 * @param queue The queue to enqueue to
 * @param triangle The triangle to throw into the queue
 * @return true if successful; false otherwise
 */
bool queue_enque(struct Queue *queue, struct Point *triangle);

/**
 * @brief Thread-safe dequeues a triangle from the queue
 * 
 * @param queue The queue to dequeue from
 * @param out_triangle A pointer to a point-array to store the result in.
 * @return true if successful; false otherwise
 */
bool queue_dequeue(struct Queue *queue, struct Point *out_triangle);

/* - main program - */

#define INPUT_BUFSZ 255
#define INIT_INPUT  ""
#define PROMPT      "" 
#define ACCEPT_NEGATIVE_COORDS true

/* Queue for triangle calculations*/
struct Queue triangle_queue;

/**
 * @brief Initialisiert den ganzen Semaphorenquatsch.
 * 
 * @param threads Maximum number of worker threads
 * @return true Alles supi
 * @return false Irgendwas kaputt
 */
bool init_semaphores(int threads)
{
    /* initialise the output semaphore */
    if (sem_init(&sem_output_wait, 0, 0)) {
        return false;
    }
    if (sem_init(&sem_worker_dispatch, 0, threads)) {
        return false;
    }
    if (sem_init(&sem_state_lock, 0, 1)) {
        return false;
    }
    return true;
}

/**
 * @brief Start routine of our worker threads of this problem.
 * Remember the threads are meant to run detached.
 * This has the advantage that no join is necessary but 
 * you need to bookkeep yourself if a thread has finished its workload.
 * @param param the param of our worker threads
 */
static void *worker(void *param)
{   
    for (;;) {
        /* dequeue a point when avaliable */
        struct Point points[3];
        if (!queue_dequeue(&triangle_queue, points)) {
            fputs("Ich kann mich nicht dequeuen! Mach was\n", stderr);
        }

        /* increase active work count*/
        sem_wait(&sem_state_lock);
        state.active += 1;
        sem_post(&sem_state_lock);
        sem_post(&sem_output_wait); // notify printer about changed state...

        /* das geht, weil struct Point[3] binär-kompatibel zu struct triangle aus triangle.h ist. */
        countPoints((struct triangle *)points, &calc_finished_cb);

        /* update stats, exit thread */
        sem_wait(&sem_state_lock);
        state.active -= 1;
        state.finished += 1;
        sem_post(&sem_state_lock);
        sem_post(&sem_output_wait); // notify printer about changed state...
    }

    return NULL;
}

int main(int argc, char **argv)
{
#if DEBUG
    puts("Patric Alt\n-> has always <num_threads> threads\n");
#endif

    /* ich brauche genau ein argument, also 2 lul */
    if (argc != 2) {
        fprintf(stderr, "Expected 1 argument but got %d.\n", argc - 1);
        return -1;
    }

    /* parse number of threads */
    int num_threads;
    struct ParseError perr = { .ln = argv[1] };
    if (!parse_int(argv[1], &num_threads, &perr, false)) {
        parse_error_print(&perr);
        return -1;
    } else if (num_threads == 0) {
        fputs("Was soll ich mit 0 threads machen, huh?", stderr);
        return -1;
    }

    /* semaphoren hier zu initialisieren wäre zu einfach und würde nen stackframe sparen... */
    if (!init_semaphores(num_threads)) {
        perror("Ich kann meine semaphoren nicht semaphorieren!");
        return -1;
    }

    /* Falls Schlange nicht vorhanden */
    if (!queue_init(&triangle_queue)){
        perror("Mensa heute geschlossen, Warten gibts heute net");
        return -1;
    }

    for (size_t i = 0; i < num_threads; ++i) {
        pthread_t thread;
        if (pthread_create(&thread, NULL, &worker, NULL)) {
            perror("Threads sind ausverkauft!");
            return -1;
        }
    }

    /* Start the output printer */
    pthread_t printer_thread;
    if (pthread_create(&printer_thread, NULL, &printer, NULL)) {
        perror("Der Drucker streikt");
        return -1;
    }
    /* detatch mulm threads */
    pthread_detach(printer_thread);

    /* read input */
    char buf[INPUT_BUFSZ];
    char *bufptr = buf;
    strncpy(buf, INIT_INPUT, INPUT_BUFSZ);
    struct Point points[3];
    for (;;) {
        //scanf("(%d,%d),(%d,%d),(%d,%d)", &x1, &y1, &x2, &y2, &x3, &y3); // We don't do that here.
        if (read_next_input(buf, &bufptr, INPUT_BUFSZ, points)) {
            /* TODO: process input */
            // print_triangle(points);
            queue_enque(&triangle_queue, points);
        }
    }

    return 1337;
}

/* - point und dreieckskram - */

int print_triangle(const struct Point *points)
{
    return printf("(%d, %d), (%d, %d), (%d, %d)\n",
        points[0].x, points[0].y,
        points[1].x, points[1].y,
        points[2].x, points[2].y
    );
}

/* - Ein wenig Kesselplatte - */

/**
 * @brief Checks if a char is an EOL delimiter
 * 
 * @param c The char to check
 * @return true if is EOL delim; false otherwise.
 */
bool parser_iseol(char c)
{
    return c == '\0' || c == '\n' || c == ';';
}

/**
 * @brief Checks if a char is any delimiter
 * 
 * @param c The char to check
 * @return true if is any delim; false otherwise.
 */
bool parser_isdelim(char c)
{
    return c == ',' || c == ')' || parser_iseol(c);
}

#define PARSER_SKIP_WHITESPACES(c) while (*c == ' ') ++c;
#define PARSER_SKIP_TO_EOL(c) while (!parser_iseol(*c)) ++c;
#define PARSER_SKIP_TO_DELIM(c) while (*c != '\0' && !parser_isdelim(*c)) ++c;

/**
 * @brief Sets parsing error inforamtion.
 * 
 * @param parse_error The parse-information output struct
 * @param from A pointer to the char an error begins at.
 * @param to A pointer to the char an error ends at. 
 * @param msg The error-message
 */
void parse_error_set(struct ParseError *parse_error, char *from, char *to, const char *msg)
{
    parse_error->from    = from;
    parse_error->to      = to;
    parse_error->message = msg;
}

void parse_error_print(struct ParseError *parse_error)
{
    size_t bufsz = 8;
    char *lnbuf = malloc(bufsz);
    char *msgbuf = malloc(bufsz);
    size_t i = 0;
    for (const char *ptr = parse_error->ln; !parser_iseol(*ptr) || ptr < parse_error->to; ++ptr) {
        if (i + 1 == bufsz) {
            bufsz *= 1.5;
            lnbuf = realloc(lnbuf, bufsz);
            if (ptr < parse_error->to) {
                msgbuf = realloc(msgbuf, bufsz);
            }
        }

        if (ptr == parse_error->from) {
            msgbuf[i] = '^';
        } else if (ptr > parse_error->from && ptr < parse_error->to) {
            msgbuf[i] = '~';
        } else if (ptr == parse_error->to) {
            msgbuf[i] = '\0';
        } else if (ptr < parse_error->from){
            msgbuf[i] = ' ';
        }
        lnbuf[i] = *ptr == '\n' ? ' ' : *ptr;
        ++i;
    }
    lnbuf[i] = '\0';

    fprintf(stderr, "parse error:\n");
    fprintf(stderr, "  | %s\n", lnbuf);
    fprintf(stderr, "  | %s %s\n", msgbuf, parse_error->message);

    free(lnbuf);
    free(msgbuf);
}

char *parse_int(char *str, int *out_i, struct ParseError *out_error, bool allow_neg)
{
    *out_i = 0;
    char *ptr = str;
    int mult = 1;

    if (allow_neg) {
        if (*ptr == '-') {
            mult = -1;
            ptr++;
        }
        PARSER_SKIP_WHITESPACES(ptr);
    }

    for (; *ptr != '\0' && !parser_isdelim(*ptr); ++ptr) {
        if (!isdigit(*ptr)) {
            PARSER_SKIP_TO_DELIM(ptr);
            goto err;
        }
        *out_i *= 10;
        *out_i += *ptr - '0';
    }
    if (ptr == str) {
        goto err;
    }
    *out_i *= mult;
    return ptr;

err:
    if (!allow_neg) {
        parse_error_set(out_error, str, ptr, "expected non-negative integer.");
    } else {
        parse_error_set(out_error, str, ptr, "expected integer.");
    }
    return NULL;
}

char *parse_point(char *str, struct Point *out_point, struct ParseError *out_error)
{
    char *ptr = str;

    /* expect '(' */
    if (*ptr != '(') {
        parse_error_set(out_error, ptr, ptr + 1, "expected '('.");
        return NULL;
    }
    ++ptr;
    PARSER_SKIP_WHITESPACES(ptr);

    /* expect and parse integer */
    ptr = parse_int(ptr, &out_point->x, out_error, ACCEPT_NEGATIVE_COORDS);
    if (ptr == NULL) {
        return NULL;
    }
    PARSER_SKIP_WHITESPACES(ptr);

    /* expect ',' */
    if (*ptr != ',') {
        parse_error_set(out_error, ptr, ptr + 1, "expected ','.");
        return NULL;
    }
    ++ptr;
    PARSER_SKIP_WHITESPACES(ptr);

    /* expect and parse integer */ 
    ptr = parse_int(ptr, &out_point->y, out_error, ACCEPT_NEGATIVE_COORDS);
    if (ptr == NULL) {
        return NULL;
    }
    PARSER_SKIP_WHITESPACES(ptr);

    /* expect ')' */
    if (*ptr != ')') {
        parse_error_set(out_error, ptr, ptr + 1, "expected ')'.");
        return NULL;
    }
    ++ptr;

    return ptr;
}

char *parse_triangle(char *str, struct Point *out_points, struct ParseError *out_error)
{
    char *ptr = str;

    for (size_t i = 0; i < 3; ++i) {
        PARSER_SKIP_WHITESPACES(ptr);
        
        /* expect and parse point */
        ptr = parse_point(ptr, out_points + i, out_error);
        if (ptr == NULL) {
            return NULL;
        }

        PARSER_SKIP_WHITESPACES(ptr);
        
        /* expect ',' */
        if (i != 2 && *ptr != ',') {
            parse_error_set(out_error, ptr, ptr + 1, "expected ','.");
            return NULL;
        }

        if (i != 2) {
            ++ptr;
        }
    }

    PARSER_SKIP_WHITESPACES(ptr);

    if (*ptr != '\n' && *ptr != '\0' && *ptr != ';') {
        printf("dbg: %s\n", ptr);
        char *to = ptr;
        PARSER_SKIP_TO_EOL(to);
        parse_error_set(out_error, ptr, to, "expected end of line.");
        return NULL;
    }

    return ptr;
}

bool read_next_input(char *buf, char **last, size_t bufsz, struct Point *out_points)
{
    struct ParseError perr = {};

    if (**last == '\0') {
        puts(PROMPT);
        fflush(stdout);
        /* read next line, maybe use readline() on linux/unix */
        if (fgets(buf, bufsz, stdin) == NULL) {
            if (feof(stdin)) {
                /* eof reached (ctrl+d) */
                puts("\n");
                exit(0);
            } else {
                /* error reading from stdin */
                perror("Error occured reading from stdin.");
                exit(-1);
            }
        }
        *last = buf;
    }

    while (**last != '\0' && parser_iseol(**last)) {
        (*last)++;
    }
    PARSER_SKIP_WHITESPACES(*last);
    if (**last == '\0') {
        return false;
    }
    perr.ln = *last;
    *last = parse_triangle(*last, out_points, &perr);
    if (*last == NULL) {
        parse_error_print(&perr);
        *last = perr.ln;
        PARSER_SKIP_TO_EOL(*last);
        return false;
    }

    return true;
}

/* - queue - */

struct QueueElem {
    struct Point points[3];
    struct QueueElem *prev;
    struct QueueElem *next;
};

static bool queue_elem_init(struct QueueElem *qelem, struct QueueElem *prev)
{
    qelem->next = NULL;
    qelem->prev = prev;
    if (prev != NULL) {
        prev->next = qelem;
    }
    return true;
}

bool queue_init(struct Queue *queue)
{
    /* Semaphore for the queue length */
    if (sem_init(&queue->sem_elements, 0, 0) != 0) {
        return false;
    }

    /* Mutex for queue access */
    if (sem_init(&queue->lock, 0, 1) != 0) {
        return false;
    }
    queue->head = malloc(sizeof(*queue->head));
    if (queue->head == NULL) {
        return false;
    }
    queue->end = queue->head;
    if (!queue_elem_init(queue->end, NULL)){
        return false;
    }
    return true;
}

bool queue_enque(struct Queue *queue, struct Point *triangle)
{
    sem_wait(&queue->lock);
    
    memcpy(queue->end->points, triangle, sizeof(struct Point) * 3);

    struct QueueElem *new = malloc(sizeof(*new));
    if (!queue_elem_init(new, queue->end)) {
        sem_post(&queue->lock); // unnötig aber toll
        return false;
    }
    queue->end = new;

    sem_post(&queue->lock);

    sem_post(&queue->sem_elements);

    return true;
}

bool queue_dequeue(struct Queue *queue, struct Point *out_triangle)
{
    sem_wait(&queue->sem_elements);

#ifdef DEBUG
    if (queue->head == queue->end) {
        fputs("dbg: head = end.", stderr);
        exit(-1);
    }
#endif

    sem_wait(&queue->lock);
    //memmove(out_triangle, queue->head->points, sizeof(struct Point) * 3);
    out_triangle[0] = queue->head->points[0];
    out_triangle[1] = queue->head->points[1];
    out_triangle[2] = queue->head->points[2];
    struct QueueElem *old = queue->head;
    queue->head = queue->head->next;
    free(old);
    sem_post(&queue->lock);

    return true;
}