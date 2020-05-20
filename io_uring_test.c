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
#define IO_BLOCK_SIZE  1024 * 1024 * 2

struct ring {
    int ring_fd;
    struct io_uring_params* params;
    void* submission_queue_base;

    struct io_uring_sqe* submission_queue_entries;
    struct io_uring_sqe* completion_queue;
};

struct ring* setup_ring();

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(
                stderr,
                "Usage: %s <filename1> [<filename2> ...]\n",
                argv[0]
        );
        return 1;
    }

    struct ring* result = setup_ring();
    if (result == NULL) {
        return 1;
    }

    struct io_uring ring;
    int ret = io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
    if (ret != 0) {
        perror("queue init");
        return 1;
    }

    char* filename = argv[1];
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);

    struct statx x1;
    io_uring_prep_statx(
            sqe,
            0,
            filename,
            0,
            STATX_ALL,
            &x1
    );

    io_uring_sqe_set_data(
            sqe,
            0
    );

    ret = io_uring_submit(&ring);
    if (ret == 0) {
        perror("queue submit");
        return 1;
    }

    struct io_uring_cqe* cqe;
    ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
        perror("io_uring_wait_cqe");
        return 1;
    }

    if (cqe->res < 0) {
        fprintf(stderr, "statx failed.\n");
        return 1;
    }

    fprintf(
            stdout,
            "File size: %llu\n",
            x1.stx_size);
    io_uring_cqe_seen(&ring, cqe);

    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_openat(
            sqe,
            0,
            filename,
            0,
            O_RDONLY);

    io_uring_sqe_set_data(
            sqe,
            0
    );

    ret = io_uring_submit(&ring);
    if (ret == 0) {
        perror("queue submit");
        return 1;
    }

    ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
        perror("io_uring_wait_cqe");
        return 1;
    }

    int file_fd = cqe->res;
    if (file_fd < 0) {
        fprintf(stderr, "openat failed.\n");
        return 1;
    }
    io_uring_cqe_seen(&ring, cqe);

    struct iovec* io_vector = malloc(sizeof(struct iovec));
    io_vector->iov_len = x1.stx_size;
    if (posix_memalign(&io_vector->iov_base, 1024, x1.stx_size)) {
        perror("posix_memalign");
        return 1;
    }
//    io_vector->iov_len = IO_BLOCK_SIZE;
//    if (posix_memalign(&io_vector->iov_base, IO_BLOCK_SIZE, IO_BLOCK_SIZE)) {
//        perror("posix_memalign");
//        return 1;
//    }

    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_readv(
            sqe,
            file_fd,
            io_vector,
            1,
            0
    );

    io_uring_sqe_set_data(
            sqe,
            0
    );

    ret = io_uring_submit(&ring);
    if (ret == 0) {
        perror("queue submit");
        return 1;
    }

    ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
        perror("io_uring_wait_cqe");
        return 1;
    }

    if (cqe->res < 0) {
        fprintf(stderr, "readv failed.\n");
        return 1;
    }
    io_uring_cqe_seen(&ring, cqe);

    output_to_console(io_vector->iov_base, cqe->res);
}

struct ring* setup_ring() {
    struct io_uring_params* params;
    params = malloc(sizeof(*params));

    int ring_fd = io_uring_setup(QUEUE_DEPTH, params);
    if (ring_fd < 0) {
        perror("io_uring_setup");
        return NULL;
    }

    void* submission_queue_base = mmap(
            0,
            params->sq_off.array + params->sq_entries * sizeof(__u32),
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_POPULATE,
            ring_fd,
            IORING_OFF_SQ_RING);
    if (submission_queue_base == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    struct io_uring_sqe* submission_queue_entries = mmap(
            0,
            params->sq_entries * sizeof(struct io_uring_sqe),
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_POPULATE,
            ring_fd,
            IORING_OFF_SQES
    );
    if (submission_queue_entries == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    struct io_uring_sqe* completion_queue = mmap(
            0,
            params->cq_off.cqes + params->cq_entries * sizeof(struct io_uring_cqe),
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_POPULATE,
            ring_fd,
            IORING_OFF_CQ_RING
    );
    if (completion_queue == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    struct ring* result = malloc(sizeof(struct ring));
    if (result == NULL) {
        perror("malloc failed");
        return NULL;
    }

    result->ring_fd = ring_fd;
    result->params = params;
    result->completion_queue = completion_queue;
    result->submission_queue_base = submission_queue_base;
    result->submission_queue_entries = submission_queue_entries;

//    unsigned* p_tail = submission_queue_base + params->sq_off.tail;
//    unsigned* p_head = submission_queue_base + params->sq_off.head;
//    unsigned* p_ring_mask = submission_queue_base + params->sq_off.ring_mask;
//    unsigned* p_dropped = submission_queue_base + params->sq_off.dropped;
//    unsigned* p_flags = submission_queue_base + params->sq_off.flags;
//    unsigned* p_ring_entries = submission_queue_base + params->sq_off.ring_entries;
//
//    unsigned tail = *p_tail;
//    unsigned head = *p_head;
//    unsigned ring_mask = *p_ring_mask;
//    unsigned dropped = *p_dropped;
//    unsigned flags = *p_flags;
//    unsigned ring_entries = *p_ring_entries;

//    __u32 index = tail & mask;
//    submission_queue_indices[index] = index;
//    submission_queue_entries[index];

    return result;
}
