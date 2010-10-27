/* Usage:
cromalarm [-q] [-c <config-file>] [-v ...]
  -q - Query Data Streams
  -c - File that holds configuration data (default: cromalarm.conf)
  -v - increases the verbosity by one level */

#include<stdio.h>
#include<stdlib.h> /* exit() */
#include<string.h> /* strtok(), strcmp(), strcpy() */

/* The following includes are for the queryds() function */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> /* gethostbyname() */

/* Needed for close function (detach from console) */
#include <sys/stat.h>
#include <fcntl.h>

/* For the decode packet function */
#include <math.h> /* pow() */

/* For log_message() function */
#include <time.h>
#include <locale.h>
#include <langinfo.h>

/* For forking */
#include <errno.h>
#include <unistd.h>

/* Signal Handling Functions */
#include <signal.h>
#include <stddef.h>
#include <sys/wait.h>

/* Default configuration filename */
char conf_file[4096] = "/etc/chromad/alert-chroma.conf";

/* Default config variables */
char Croma_Host[83] = "10.0.0.1";
char Local_Port[5] = "65496";
char Alarms[64] = "2222112222";
char SMTP_Host[83] = "localhost";
char Notify[1024] = "root@localhost";
char Logfile[4096] = "/var/log/chromad/chroma.log";
char Channelfile[4096] = "/etc/chromad/chroma-channels.conf";
char temp_message_space[1024];
int Delay = 20;
int DataRate = 1;

/* Buffer for received UDP data */
unsigned char alarm_buffer[140];

/* Global variables to allow recall of get_alarm_packet function */
int udp_sock, pcount = 0;

/* Variable set for decoder section */
unsigned char stereo_mask[4], mono_mask[8];

/* Audio Loss Counter Matrices */
int mono_delay_counter[8][8], stereo_delay_counter[4][8];

/* Array to be populated with channel names */
char chan_line[96][80];

/* Default verbose level */
int verbosity = 0;

int read_config(char *conf_file);

int read_channelfile(void);

int log_message(int level, char *message);

int queryds(void);

int startds(void);

int haltds(void);

void log_pcount_value(int sig);

void logrotate_ack(int sig);

void exit_gracefully(int sig);

void child_is_done(int sig);

int get_alarm_packet(void);

int build_masks(void);

int email(char *message);

int notify(int i, int state, char mors);

int delay_ok(int i, unsigned char alarm_result, char mors);

int check_alarms(void);
