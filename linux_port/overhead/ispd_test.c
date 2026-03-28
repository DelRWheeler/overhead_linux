/*
 * ispd_test.c - Minimal standalone ISPD streaming test in C.
 * Proves whether C termios serial I/O can stream from the ISPD.
 *
 * Build:  gcc -o ispd_test ispd_test.c
 * Run:    ./ispd_test /dev/ttyS0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <time.h>
#include <errno.h>

static int open_serial(const char *path, int baud)
{
    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    /* Clear O_NONBLOCK */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    /* Use termios2 for baud rate (same as controller Serial.cpp) */
    struct {
        unsigned int c_iflag, c_oflag, c_cflag, c_lflag;
        unsigned char c_line, c_cc[19];
        unsigned int c_ispeed, c_ospeed;
    } tio;

    #define TCGETS2 _IOR('T', 0x2A, typeof(tio))
    #define TCSETS2 _IOW('T', 0x2B, typeof(tio))

    if (ioctl(fd, TCGETS2, &tio) < 0) {
        perror("TCGETS2");
        close(fd);
        return -1;
    }

    /* Raw input */
    tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
                      INLCR | IGNCR | ICRNL | IXON | IXOFF);

    /* Raw output */
    tio.c_oflag &= ~OPOST;

    /* 8N1, receiver on, local, no HW flow control */
    tio.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
    tio.c_cflag |= CS8 | CREAD | CLOCAL;

    /* Raw mode */
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

    /* VMIN=1 VTIME=1 (100ms inter-byte timeout) */
    tio.c_cc[6] = 1;   /* VMIN */
    tio.c_cc[5] = 1;   /* VTIME */

    /* Baud rate via BOTHER */
    tio.c_cflag &= ~0x100F;  /* Clear CBAUD */
    tio.c_cflag |= 0x1000;   /* BOTHER */
    tio.c_ispeed = baud;
    tio.c_ospeed = baud;

    if (ioctl(fd, TCSETS2, &tio) < 0) {
        perror("TCSETS2");
        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);

    /* Assert DTR + RTS (same as controller Serial.cpp) */
    int bits = TIOCM_DTR | TIOCM_RTS;
    ioctl(fd, TIOCMBIS, &bits);

    return fd;
}

static int send_cmd(int fd, const char *cmd, const char *label, int wait_ms)
{
    /* Flush input */
    tcflush(fd, TCIFLUSH);

    /* Write command */
    int len = strlen(cmd);
    int n = write(fd, cmd, len);
    if (n != len) {
        printf("  %s: write failed (%d/%d)\n", label, n, len);
        return -1;
    }

    /* Wait for response */
    usleep(wait_ms * 1000);

    /* Read response */
    int avail = 0;
    ioctl(fd, FIONREAD, &avail);

    char buf[256] = {0};
    if (avail > 0) {
        int r = read(fd, buf, avail < 255 ? avail : 255);
        if (r > 0) {
            /* Strip CR/LF for display */
            for (int i = 0; i < r; i++)
                if (buf[i] == '\r' || buf[i] == '\n') buf[i] = ' ';
            buf[r] = 0;
        }
    }
    printf("  %-30s -> [%s] (%d bytes)\n", label, buf, avail);
    return avail;
}

int main(int argc, char **argv)
{
    const char *port = argc > 1 ? argv[1] : "/dev/ttyS0";

    printf("=== ISPD C Serial Test ===\n");
    printf("Port: %s  Baud: 115200\n\n", port);

    int fd = open_serial(port, 115200);
    if (fd < 0) return 1;
    printf("Serial port opened OK (fd=%d)\n\n", fd);

    /* Send OP 1 */
    printf("--- Commands ---\n");
    send_cmd(fd, "OP 1\r", "OP 1 (open device)", 300);
    send_cmd(fd, "ID\r", "ID (identity)", 300);
    send_cmd(fd, "UR\r", "UR (query update rate)", 300);

    /* Flush before streaming */
    tcflush(fd, TCIOFLUSH);

    /* Send SX to start streaming */
    printf("\n--- SX Stream Test (5 seconds) ---\n");
    write(fd, "SX\r", 3);
    printf("  SX sent\n");

    /* Read stream for 5 seconds */
    char line[64];
    int line_idx = 0;
    int sample_count = 0;
    int total_bytes = 0;
    int empty_polls = 0;
    time_t start = time(NULL);
    struct timespec ts_start, ts_now;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        double elapsed = (ts_now.tv_sec - ts_start.tv_sec) +
                         (ts_now.tv_nsec - ts_start.tv_nsec) / 1e9;
        if (elapsed >= 5.0) break;

        /* Check available bytes */
        int avail = 0;
        ioctl(fd, FIONREAD, &avail);

        if (avail > 0) {
            unsigned char buf[1024];
            int to_read = avail < (int)sizeof(buf) ? avail : (int)sizeof(buf);
            int n = read(fd, buf, to_read);
            if (n > 0) {
                total_bytes += n;
                /* Parse lines */
                for (int i = 0; i < n; i++) {
                    if (buf[i] == '\r') {
                        line[line_idx] = 0;
                        if (line_idx >= 3 && (line[0] == 'S' || line[0] == 's')) {
                            sample_count++;
                            if (sample_count <= 3 || sample_count % 500 == 0) {
                                printf("  sample #%d at %.2fs: %s\n",
                                       sample_count, elapsed, line);
                            }
                        } else if (line_idx > 0) {
                            printf("  non-sample at %.2fs: [%s]\n", elapsed, line);
                        }
                        line_idx = 0;
                    } else if (buf[i] == '\n') {
                        /* skip LF */
                    } else {
                        if (line_idx < 62)
                            line[line_idx++] = buf[i];
                    }
                }
            }
            empty_polls = 0;
        } else {
            empty_polls++;
            if (empty_polls == 1 && sample_count > 0)
                printf("  (first empty poll after %d samples at %.2fs)\n",
                       sample_count, elapsed);
            usleep(1000); /* 1ms */
        }
    }

    /* Stop streaming */
    write(fd, "GS\r", 3);

    clock_gettime(CLOCK_MONOTONIC, &ts_now);
    double total_time = (ts_now.tv_sec - ts_start.tv_sec) +
                        (ts_now.tv_nsec - ts_start.tv_nsec) / 1e9;

    printf("\n--- Results ---\n");
    printf("  Samples:    %d\n", sample_count);
    printf("  Duration:   %.1f seconds\n", total_time);
    printf("  Rate:       %.0f samples/sec\n", sample_count / total_time);
    printf("  Total bytes: %d\n", total_bytes);
    printf("  Status:     %s\n",
           sample_count > 100 ? "STREAMING OK" : "STREAMING FAILED");

    close(fd);
    return 0;
}
