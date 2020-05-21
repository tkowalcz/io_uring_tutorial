#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <liburing.h>
#include <sys/mman.h>
#include <include/uapi/linux/io_uring.h>
#include <include/uapi/linux/stat.h>
#include <sys/uio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "stuff.h"

#define QUEUE_DEPTH 1024
#define IO_BLOCK_SIZE  16
#define LOOP_EXIT -1

struct io_uring* ring;

enum op {
    INIT, STAT, OPEN, READ, PRINT
};

struct continuation {
    enum op operation;

    char* filename;
    struct statx* stat;
    int fd;
    struct iovec* io_vector;
    size_t bytes_processed;
};

int event_handler(struct continuation* cont, __s32 cqe_res) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);

    switch (cont->operation) {
        case INIT: {
            cont->operation = STAT;
            cont->stat = malloc(sizeof(struct statx));

            io_uring_prep_statx(
                    sqe,
                    0,
                    cont->filename,
                    0,
                    STATX_ALL,
                    cont->stat
            );
            break;
        }

        case STAT: {
            cont->operation = OPEN;

            fprintf(
                    stdout,
                    "File size: %llu\n",
                    cont->stat->stx_size);

            io_uring_prep_openat(
                    sqe,
                    0,
                    cont->filename,
                    0,
                    O_RDONLY);
            break;
        }

        case OPEN: {
            cont->operation = READ;
            cont->fd = cqe_res;

            io_uring_prep_readv(
                    sqe,
                    cont->fd,
                    cont->io_vector,
                    1,
                    0
            );
            break;
        }

        case READ: {
            cont->operation = PRINT;

            io_uring_prep_writev(
                    sqe,
                    STDOUT_FILENO,
                    cont->io_vector,
                    1,
                    0
            );

            cont->bytes_processed = cont->bytes_processed + cqe_res;
            break;
        }

        case PRINT: {
            memset(
                    cont->io_vector->iov_base,
                    0,
                    cont->io_vector->iov_len
            );

            if (cont->bytes_processed < cont->stat->stx_size) {
                cont->operation = READ;
                io_uring_prep_readv(
                        sqe,
                        cont->fd,
                        cont->io_vector,
                        1,
                        cont->bytes_processed
                );
                break;
            }

            return LOOP_EXIT;
        }
    }

    io_uring_sqe_set_data(
            sqe,
            cont
    );
    int ret = io_uring_submit(ring);
    if (ret == 0) {
        perror("queue submit");
        return 1;
    }

    return 0;
}

int event_loop() {
    struct io_uring_cqe* cqe;

    while (1) {
        int ret = io_uring_wait_cqe(
                ring,
                &cqe
        );
        if (ret < 0) {
            perror("io_uring_wait_cqe");
            return 1;
        }

        struct continuation* cont = io_uring_cqe_get_data(cqe);
        if (cqe->res < 0) {
            fprintf(
                    stderr,
                    "%d failed.\n",
                    cont->operation
            );
            return 1;
        }

        ret = event_handler(cont, cqe->res);
        if (ret == LOOP_EXIT) {
            return LOOP_EXIT;
        }

        io_uring_cqe_seen(
                ring,
                cqe
        );
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(
                stderr,
                "Usage: %s <filename1> [<filename2> ...]\n",
                argv[0]
        );
        return 1;
    }

    ring = malloc(sizeof(struct io_uring));
    int ret = io_uring_queue_init(QUEUE_DEPTH, ring, 0);
    if (ret != 0) {
        perror("queue init");
        return 1;
    }

    struct continuation* cont = malloc(sizeof(struct continuation));
    cont->operation = INIT;
    cont->filename = argv[1];

    struct iovec* io_vector = malloc(sizeof(struct iovec));
    io_vector->iov_len = IO_BLOCK_SIZE;
    io_vector->iov_base = malloc(IO_BLOCK_SIZE);
    if (io_vector->iov_base == NULL) {
        perror("malloc");
        return 1;
    }

    cont->io_vector = io_vector;

    event_handler(cont, 0);
    event_loop();
}
