/*
 * Copyright (c) 2015, WigWag.com
 * Copyright (c) 2012, Maxim Osipov <maxim.osipov@gmail.com>
 * Copyright (c) 2010, Mariano Alvira <mar@devl.org> and other contributors
 * to the MC1322x project (http://mc1322x.devl.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * WigWag additions:
 * -will not run perpetually when failing to connect
 * -fixed baud rate setting from command line (was not set before)
 * -made it quieter when not specifying verbose
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>


char* filename;
char* second;
char* term = "/dev/ttyUSB0";
int baud = B115200;
int verbose = 0;
char* rts = "none";
char* command;
int first_delay = 50;
int second_delay = 100;
int do_exit = 0;
int zerolen = 0;
int ownlen = 0;
char *args = NULL;

struct stat sbuf;
struct termios options;
char buf[256];
int pfd;
int ffd;
int sfd;
int silent=0;

void help(void);
void waitFor(char *command, const char *needle, const char sendZero);
void reset(char *command) {
   /* Reset the board if we can */
   if (verbose) printf("Reset the board to enter bootloader\n");
  if (command) {
    if (verbose)  printf("Performing reset: %s\n", command);
    system(command);
  }
}

int main(int argc, char **argv) {
  int c = 0;
  int r = 0;
  int i = 0;
  uint32_t s = 0;
  opterr = 0;

  /* Parse options */
  while ((c = getopt(argc, argv, "f:s:zlt:vu:r:c:a:b:eh")) != -1) {
    switch (c)
      {
      case 'f':
        filename = optarg;
        break;
      case 's':
        second = optarg;
        break;
      case 'z':
        zerolen = 1;
        break;
      case 'l':
        ownlen = 1;
        break;
      case 't':
        term = optarg;
        break;
      case 'v':
        verbose = 1;
        break;
      case 'u':
        if (! strcmp(optarg, "115200")) {
          baud = B115200;
        } else if (! strcmp(optarg, "230400")) {
          baud = B230400;
        } else if (! strcmp(optarg, "921600")) {
          baud = B921600;
        } else if (! strcmp(optarg, "57600")) {
          baud = B57600;
        } else if (! strcmp(optarg, "19200")) {
          baud = B19200;
        } else if (! strcmp(optarg, "9600")) {
          baud = B9600;
        } else {
          printf("Unknown baud rate %s!\n", optarg);
          return -1;
        }
        break;
      case 'r':
        rts = optarg;
        break;
      case 'c':
        command = optarg;
        break;
      case 'a':
        first_delay = atoi(optarg);
        break;
      case 'b':
        second_delay = atoi(optarg);
        break;
      case 'e':
        do_exit = 1;
        break;
      case 'h':
      case '?':
        help();
        return 0;
      default:
        abort();
      }
  }
  /* Get other arguments */
  if (optind < argc)
    args = argv[optind];

  /* Print settings */
  if (verbose) {
    printf("Primary file (RAM): %s\n", filename);
    printf("Secondary file (Flash): %s\n", second);
    printf("Zero secondary file: %s\n", zerolen == 1 ? "Yes" : "No");
    printf("Port: %s\n", term);
    printf("Baud rate: %i\n", baud);
    printf("Flow control: %s\n", rts);
    printf("Reset command: %s\n", command);
    printf("Exit after load: %s\n", do_exit == 1 ? "Yes" : "No");
    printf("Delay 1: %i\n", first_delay);
    printf("Delay 2: %i\n", second_delay);
  }

  /* Open and configure serial port */
  pfd = open(term, O_RDWR | O_NOCTTY | O_NDELAY);
  if (pfd == -1) {
    printf("Cannot open serial port %s!\n", term);
    return -1;
  }
  fcntl(pfd, F_SETFL, FNDELAY);
  tcgetattr(pfd, &options);
  cfsetispeed(&options, baud);
  options.c_cflag |= (CLOCAL | CREAD);
  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;
  if (strcmp(rts, "rts")) {
    options.c_cflag &= ~CRTSCTS;
  } else {
    options.c_cflag |= CRTSCTS;
  }
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  options.c_oflag &= ~OPOST;
  tcsetattr(pfd, TCSANOW, &options);

 reset(command);

  /* Primary bootloader wait loop */
  waitFor(command,"CONNECT", 1);

  /* Send primary file */
  if (!filename) {
    printf("Please specify firmware file name (-f option)!\n");
    return -1;
  }
  if (stat(filename, &sbuf)) {
    printf("Cannot open firmware file %s!\n", filename);
    return -1;
  }
  ffd = open(filename, O_RDONLY);
  if (ffd == -1) {
    printf("Cannot open firmware file %s!\n", filename);
    return -1;
  }
  s = sbuf.st_size;
   if (verbose) printf("Sending %s (%i bytes)...\n", filename, s);
  r = write(pfd, (const void*)&s, 4);
  i = 0;
  r = read(ffd, buf, 1);
  while (r > 0) {
    do {
      usleep(first_delay);
      c = write(pfd, (const void*)buf, r);
    } while(c < r);
    i += r;
    if (verbose) printf("Written %i\r", i); 
    fflush(stdout);
    r = read(ffd, buf, 1);
  }
  printf("\n");

  /* Secondary loader wait loop */
  if (second || zerolen) {
    /* Wait for ready */
    waitFor(command,"ready", 0);

    /* Send secondary file */
     if (verbose) printf("Sending secondary file\n");
    if (second) {
      if (stat(second, &sbuf)) {
        printf("Cannot open secondary file %s!\n", second);
        return -1;
      }
      sfd = open(second, O_RDONLY);
      if (sfd == -1) {
        printf("Cannot open secondary file %s!\n", second);
        return -1;
      }

      if (ownlen) {
        s = sbuf.st_size;
        r = write(pfd, (const void*) &s, 4);
         if (verbose) printf("Sending %s (%i bytes)...\n", second, s);
        r = read(sfd, &s, 4);
      } else {
        s = sbuf.st_size + 4;
        r = write(pfd, (const void*) &s, 4);
        s = sbuf.st_size;
        if (verbose)  printf("Sending len (4 bytes) + %s (%i bytes)...\n", second, s);
      }
      r = write(pfd, (const void*) &s, 4);

      r = read(sfd, buf, 1);
      i = 4;
      while (r > 0) {
        do {
          usleep(second_delay);
          c = write(pfd, (const void*)buf, r);
        } while(c < r);
        i += r;
        if (verbose)  printf("Written %i\r", i); fflush(stdout);
        r = read(sfd, buf, 1);
      }
      printf("\n");
    } else if (zerolen) {
      s = 0;
       if (verbose) printf("Sending %i...\n", s);
      write(pfd, (const void*)&s, 4);
    }

    /* Wait for flasher done */
    waitFor(command,"flasher done", 0);
  }

  /* Send the remaining arguments */
  if (args) {
     if (verbose) printf("Sending %s\n", args);
    r = write(pfd, (const void*)args, strlen(args));
    r = write(pfd, (const void*)",", 1);
  }

  /* Drop in echo mode */
  if (!do_exit) {
    while (1) {
      r = read(pfd, buf, sizeof(buf));
      if (r > 0) {
        buf[r] = '\0';
        printf("%s", buf); fflush(stdout);
      } 
      //      else { // ED
      //printf("[0]"); fflush(stdout);
      //}
    }
  }

  exit(EXIT_SUCCESS);
}


void help(void)
{
  printf("Example usage: mc1322x-load -f foo.bin -t /dev/ttyS0 -b 9600\n");
  printf("          or : mc1322x-load -f flasher.bin -s flashme.bin  0x1e000,0x11223344,0x55667788\n");
  printf("          or : mc1322x-load -f flasher.bin -z  0x1e000,0x11223344,0x55667788\n");
  printf("       -f required: binary file to load\n");
  printf("       -s optional: secondary binary file to send\n");
  printf("       -z optional: send a zero length file as secondary\n");
  printf("       -l optional: secondary file contains len in first 4 Bytes (little endian)\n");
  printf("       -t, terminal default: /dev/ttyUSB0\n");
  printf("       -u, baud rate default: 115200\n");
  printf("       -v, verbose (no -v silent)\n");
  printf("       -r [none|rts] flow control default: none\n");
  printf("       -c command to run for autoreset: \n");
  printf("              e.g. -c 'bbmc -l redbee-econotag -i 0 reset'\n");
  printf("       -e exit instead of dropping to terminal display\n");
  printf("       -a first  intercharacter delay, passed to usleep\n");
  printf("       -b second intercharacter delay, passed to usleep\n");
  printf("\n");
  printf("Anything on the command line is sent after all of the files.\n\n");
}


void waitFor(char *command, const char *needle, const char sendZero)
{
  int r = 0;
  int i = 0;
  int reset_count=0;
  int exit_count=0;
  int reset_after=3; 
  int exit_after=5*reset_after;  //attempt to reboot

  if (verbose)  printf("Waiting for %s...\n", needle);
  while (1) {
    if (sendZero) write(pfd, (const void*)"\0", 1);
    sleep(1);
    r = read(pfd, &buf[i], sizeof(buf)-1-i);
    if (r > 0) {
      buf[i+r] = '\0';
      printf("%s", &buf[i]); fflush(stdout);
      if (strstr(&buf[i], needle)) {
        printf("\n");
        break;
      }
      i += r;
      if (i >= sizeof(buf)-1) {
        i = 0;
      }
    } 
    else {
      printf("."); fflush(stdout); 
      if (reset_count>reset_after){
        reset(command);
        reset_count=0;
      }
      if (exit_count>exit_after){
        printf("failed to get %s\n. Must exit.  Ensure no other resources attached to TTY port.",needle);
        exit(EXIT_FAILURE);
      }
      reset_count++; exit_count++;
    }
  }
}
