/** 
 * Non-Canonical Input Processing
 * From https://tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html by Gary Frerking and Peter Baumann
**/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#include "utils.h"
#include "comms.h"

typedef enum State {START, FLAG_RCV, A_REC, C_REC, BCC_OK, STOP};


int main(int argc, char **argv)
{
  int fd, res;
  struct termios oldtio, newtio;
  char buf[255];

  int i;

  if ((argc < 2) ||
      ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
       (strcmp("/dev/ttyS1", argv[1]) != 0) &&
       (strcmp("/dev/ttyS10", argv[1]) != 0) &&
       (strcmp("/dev/ttyS11", argv[1]) != 0)))
  {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

  fd = open(argv[1], O_RDWR | O_NOCTTY);
  if (fd < 0)
  {
    perror(argv[1]);
    exit(-1);
  }

  if (tcgetattr(fd, &oldtio) == -1)
  { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 5;  /* blocking read until 5 chars received */

  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &newtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");

  //send_s_u_frame(fd, SENDER, SET);

  buf[0] = FLAG;
  buf[1] = A_SND;
  buf[2] = SET;
  buf[3] = BCC(buf[1], buf[2]);
  buf[4] = FLAG;

  write(fd, buf, 5);
  
  enum State cur_state = START;
  char a_val, c_val;

  while (cur_state != STOP) { 
    res = read(fd, buf, 1);
    printf("got: %x\n", buf[0]);
    switch (cur_state) {
        case START:
            if (buf[0] == FLAG) cur_state = FLAG_RCV;
            else printf("Unknown message byte\n");
            break;
        
        case FLAG_RCV:
            printf("rcv\n");
            if (buf[0] == A_SND){
                cur_state = A_REC;
                a_val = buf[0];
            }
            else if (buf[0] != FLAG) cur_state = START;
            break;
        
        case A_REC:
            printf("a\n");
            if (buf[0] == UA){
                cur_state = C_REC;
                c_val = buf[0];
            }
            else if (buf[0] == FLAG) cur_state = FLAG_RCV;
            else cur_state = START;
            break;
        
        case C_REC:
            printf("c\n");
            if (BCC(a_val,c_val) == buf[0])
                cur_state = BCC_OK;
            else if (buf[0] = FLAG)
                cur_state = FLAG_RCV;
            else
                cur_state = START;
            break;
        
        case BCC_OK:
            printf("bcc\n");
            if (buf[0] == FLAG)
                cur_state = STOP;
            else cur_state = START;
            break;
    }
  }

  printf("a = %x, c = %x\n", a_val, c_val);

  printf("Got confirmation. Closing...\n");

  /* Aguardar um pouco que esteja escrito tudo antes de mudar 
    a config do terminal.  */
  sleep(1);
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}