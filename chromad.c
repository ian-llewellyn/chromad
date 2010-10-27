/* Usage:
cromalarm [-q] [-c <config-file>] [-v ...]
  -q - Query Data Streams
  -c - File that holds configuration data (default: cromalarm.conf)
  -v - increases the verbosity by one level */

#include "chromad.h"

int read_config(char *conf_file) {
  /* Open the configuration file */
  FILE *fp_conf;
  if ( (fp_conf = fopen(conf_file, "r")) == NULL )
  {
    log_message(2, "Could not open configuration file for reading, using defaults");
    return 1;
  }

  /* Read in the parameters */
  /* Setup variables... */
  char conf_line[1024], *conf_var;

  while ( fgets(conf_line, 1024, fp_conf) != NULL ) {
    /* Don't bother with comment lines and blank lines */
    if ( !(conf_line[0] == '#' || conf_line[0] == 0x0A || conf_line[0] == ' ') ) {
      /* Get param name from line and terminate string with \0 */
      strtok(conf_line, " ");
      if ( !strcmp(conf_line, "Croma_Host") ) {
        conf_var = strtok(NULL, " \n");
        strcpy(Croma_Host, conf_var);
      } else if ( !strcmp(conf_line, "Local_Port") ) {
        conf_var = strtok(NULL, " \n");
        strcpy(Local_Port, conf_var);
      } else if ( !strcmp(conf_line, "Alarms") ) {
        conf_var = strtok(NULL, " \n");
        strcpy(Alarms, conf_var);
      } else if ( !strcmp(conf_line, "SMTP_Host") ) {
        conf_var = strtok(NULL, " \n");
        strcpy(SMTP_Host, conf_var);
      } else if ( !strcmp(conf_line, "Notify") ) {
        conf_var = strtok(NULL, " \n");
        strcpy(Notify, conf_var);
      } else if ( !strcmp(conf_line, "Logfile") ) {
        conf_var = strtok(NULL, " \n");
        strcpy(Logfile, conf_var);
      } else if ( !strcmp(conf_line, "Delay") ) {
        conf_var = strtok(NULL, " \n");
        Delay = atoi(conf_var);
      } else if ( !strcmp(conf_line, "Channelfile") ) {
        conf_var = strtok(NULL, " \n");
        strcpy(Channelfile, conf_var);
      } else if ( !strcmp(conf_line, "DataRate") ) {
        conf_var = strtok(NULL, " \n");
        DataRate = atoi(conf_var);
      } else {
        sprintf(temp_message_space, "Unknown configuration variable: %s", conf_line);
        log_message(2, temp_message_space);
      } /* end parameter checking if statement */
    } /* end valid config line if statement */
  } /* end while statement */

  /* Close the configuration file */
  fclose(fp_conf);

  return 0;
}

int read_channelfile(void) {
  FILE *fp_chan;
  int i = 0;
  int j = 0;

  // We'll provide some default names first and overwrite them later
  for ( i = 0; i < 96; i++ ) {
    if ( i%3 == 0 ) {
      sprintf(chan_line[i], "Stereo %d+%d", j + 1, j + 2);
    } else {
      sprintf(chan_line[i], "Channel %d", j + 1);
      j++;
    }
  }

  // Open the channel file
  if ( (fp_chan = fopen(Channelfile, "r")) == NULL )
  {
    // Log and exit the function if the file cannot be opened
    log_message(2, "Could not open channel file for reading - using default names");
    return 1;
  }

  // Reset i to 0 to fill the chan_line array from the top
  i = 0;
  while ( fgets(&chan_line[i][0], 80, fp_chan) != NULL ) {
    /* Put a null terminator in place of the last character in the string.
       This in effect removes the newline character from the end of the line.
       Doesn't work with DOS files that have CRLF line endings. */
    chan_line[i][strlen(chan_line[i])-1] = '\0';
    i++;
  }

  /* Close the configuration file */
  fclose(fp_chan);

  return 0;
}

int log_message(int level, char *message) {
  /* == ERRORS ==
     0 - fatal errors & events
     1 - critical errors (will probably become fatal)
     2 - errors
     == OPERATIONS ==
     3 - operations
     4 - repetitive operations
     == HEX DUMPS ==
     5 - operation element logging (inc. hex)
     6 - repetitive operation element logging (inc. hex)
     == ALL ==
     7 - no discrimination */
  // Does the current verbosity level catch this message?
  if ( level <= verbosity ) {
    FILE *fp_log;
    char msg_output[1024];
    time_t now;
    struct tm *l_time;
    char timestamp[20];
    pid_t pid;

    // Stick the time in a string
    time(&now);
    l_time = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%b %d %H:%M:%S", l_time);

    // Get this processes pid
    pid = getpid();

    if ( level <= 3 ) {
      // Include timestamp and pid in the log
      sprintf(msg_output, "%s [%d] %s\n", timestamp, pid, message);
    } else {
      // Verbosity levels above 3 are reserved for hex output
      // so we don't want to include timestamps or the pid.
      sprintf(msg_output, "%s\n", message);
    }

    if ( (fp_log = fopen(Logfile, "a")) == NULL ) {
      // There is a problem writing to the log file
      fprintf(stderr, "%s [%d] Can't append to %s: %s\n", timestamp, pid, Logfile, message);
      return 1;
    }

    fputs(msg_output, fp_log);

    /* Close the log file */
    fclose(fp_log);
  }

  return 0;
}

int queryds(void) {
  int sockfd, portno, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  char buffer[256];

  portno = 23;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if ( sockfd < 0 ) {
    log_message(1, "Query mode: Error opening TCP control socket");
    return 1;
  }

  server = gethostbyname(Croma_Host);
  if ( server == NULL ) {
    log_message(1, "Query mode: DNS lookup failure");
    return 1;
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
    (char *)&serv_addr.sin_addr.s_addr,
    server->h_length);

  serv_addr.sin_port = htons(portno);
  if ( connect(sockfd, &serv_addr, sizeof(serv_addr)) < 0 ) {
    log_message(1, "Query mode: Error connecting to Chromatec control port");
    return 1;
  }

  /* send the command to get a list of active data streams */
  n = write(sockfd, "DS LIST=-1\r", strlen("DS LIST=-1\r"));
  if ( n < 0 ) {
    log_message(1, "Query mode: Unable to send data to Chromatec control port");
    return 1;
  }

  /* wait a second before receiving a response (stops message fragmentation) */
  sleep(1);
  bzero(buffer, 256);
  n = read(sockfd, buffer, 255);
  if ( n < 0 ) {
    log_message(1, "Query mode: Unable to receive active Chromatec data streams");
    return 1;
  }

  printf("%s", buffer);
  bzero(buffer, 256);

  // Close the TCP socket
  close(sockfd);

  return 0;
}

int startds(void) {
  int tcp_sock, portno, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  char buffer[512];

  sprintf(buffer, "DS PORT=%s RATE=%d CHAN_LEVEL=0000000000000000 LEVEL_CONTENT=NO_LEVELS LEVEL_CONTENT=NO_VU LEVEL_CONTENT=NO_PEAK_HOLD LEVEL_CONTENT=NO_SUM_DIFF CHAN_OVER=0000000000000000 CHAN_NO_AUD=FFFFFFFFFFFFFFFF PAIR_CORR=00000000 PAIR_NO_CARRIER=00000000 PAIR_CORR_OUT=00000000 PERSIST=FOREVER\r", Local_Port, DataRate);

  portno = 23;
  tcp_sock = socket(AF_INET, SOCK_STREAM, 0);

  if ( tcp_sock < 0 ) {
    log_message(1, "Eror opening TCP control socket");
    return 1;
  }

  server = gethostbyname(Croma_Host);
  if ( server == NULL ) {
    log_message(1, "DNS lookup failure");
    return 1;
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
    (char *)&serv_addr.sin_addr.s_addr,
    server->h_length);

  serv_addr.sin_port = htons(portno);
  if ( connect(tcp_sock, &serv_addr, sizeof(serv_addr)) < 0 ) {
    log_message(1, "Error connecting to Chromatec control port");
    return 1;
  }

  /* send the command to get a list of active data streams */
  n = write(tcp_sock, buffer, strlen(buffer));
  if ( n < 0 ) {
    log_message(1, "Unable to send data to Chromatec control port");
    return 1;
  }

  /* wait a second before receiving a response (stops fragmentation) */
  sleep(1);
  bzero(buffer, 512);
  n = read(tcp_sock, buffer, 511);

  /* Tidy up the TCP connection because it's work is done */
  close(tcp_sock);

  if ( n < 0 ) {
    log_message(1, "Chromatec host failed to respond");
    return 1;
  } else {
    log_message(5, buffer);
  }

  return 0;
}

int haltds(void) {
  int tcp_sock, portno, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  char buffer[512];

  sprintf(buffer, "DS STOP=-1\r");

  portno = 23;
  tcp_sock = socket(AF_INET, SOCK_STREAM, 0);

  if ( tcp_sock < 0 ) {
    log_message(1, "Eror opening TCP control socket\n");
    return 1;
  }

  server = gethostbyname(Croma_Host);
  if ( server == NULL ) {
    log_message(1, "DNS lookup failure\n");
    return 1;
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
    (char *)&serv_addr.sin_addr.s_addr,
    server->h_length);

  serv_addr.sin_port = htons(portno);
  if ( connect(tcp_sock, &serv_addr, sizeof(serv_addr)) < 0 ) {
    log_message(1, "Error connecting to Chromatec control port\n");
    return 1;
  }

  /* send the command to get a list of active data streams */
  n = write(tcp_sock, buffer, strlen(buffer));
  if ( n < 0 ) {
    log_message(1, "Unable to send data to Chromatec control port\n");
    return 1;
  }

  /* wait a second before receiving a response (stops fragmentation) */
  sleep(1);
  bzero(buffer, 512);
  n = read(tcp_sock, buffer, 511);

  /* Tidy up the TCP connection because it's work is done */
  close(tcp_sock);

  if ( n < 0 ) {
    log_message(1, "Chromatec host failed to respond\n");
    return 1;
  } else {
    log_message(5, buffer);
  }

  return 0;
}

void log_pcount_value(int sig) {
  sprintf(temp_message_space, "Number of packets processed so far: %d", pcount);
  log_message(0, temp_message_space);

  signal(sig, log_pcount_value);
}

void logrotate_ack(int sig) {
  log_message(0, "System is Rotating Logs");

  signal(sig, logrotate_ack);
}

void exit_gracefully(int sig) {
  sprintf(temp_message_space, "Exiting on signal: %d", sig);
  log_message(0, temp_message_space);

  signal(sig, SIG_DFL);
  raise(sig);  
}

void child_is_done(int sig) {
  pid_t pid;

  /* Being lazy here. When this signal handler is called
   * the default action is restored, which is to ignore the SIGCHLD signal.
   * The while loop here repeatedly calls wait until all children
   * have returned their status and exit completely.
   * Using signalaction() is a better way to implement this code.
   */
  while ( (pid = wait(NULL)) > 0 ) {
    sprintf(temp_message_space, "Child with pid %d has exited", pid);
    log_message(3, temp_message_space);
  }

  signal(sig, child_is_done);
}

int get_alarm_packet(void) {
  int length, fromlen, n, i;
  struct sockaddr_in from;
  struct sockaddr_in server;

  if ( pcount == 0 ) {
    // If it's the first packet, we must establish a UDP socket.
    udp_sock=socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
      log_message(1, "Error opening UDP socket");
      return 1;
    }
  }

  length = sizeof(server);
  bzero(&server,length);

  server.sin_family=AF_INET;
  server.sin_addr.s_addr=INADDR_ANY;
  server.sin_port=htons(atoi(Local_Port));

  if ( pcount == 0 ) {
    // If it's the first packet, we must bind to the UDP port.
    if ( bind(udp_sock, (struct sockaddr *)&server, length) < 0 ) {
      sprintf(temp_message_space, "Error binding to UDP port %d", Local_Port);
      log_message(1, temp_message_space);
      return 1;
    }
  }

  fromlen = sizeof(struct sockaddr_in);

  n = recvfrom(udp_sock,alarm_buffer,140,0,(struct sockaddr *)&from,&fromlen);
  pcount++;

  if (n < 0) {
    log_message(1, "Error getting data from UDP socket");
    return 1;
  }

  // This while will log the actual packet contents if v is high enough
  i = 0;
  while ( i < n ) {
    if ( i == 0 ) {
      sprintf(temp_message_space, "Number of packets so far: %d\nUDP packet size: %d", pcount, n);
      log_message(4, temp_message_space);
    }

    sprintf(temp_message_space, "%d:\t0x%.2x\t%u\t-%c-", i, alarm_buffer[i], alarm_buffer[i], alarm_buffer[i]);
    log_message(6, temp_message_space);
    i++;
  }

  return 0;
}

int build_masks(void) {
  int bit_position = 0;
  unsigned char i = 0;

  /* Clear the masks before building them */
  bzero(&mono_mask, 8);
  bzero(&stereo_mask, 4);

  /* Go through the input mask character by character */
  while( i < strlen(Alarms) ) {
    if ( (int)Alarms[i] == '\x32' ) {
      /* 2 - switch on the relevant bin in the stereo mask */
      stereo_mask[bit_position/16] = stereo_mask[bit_position/16] + (int)pow(2, ((bit_position%16)/2));
      /* Part of the stereo mask, so increment the output bit counter by an extra one */
      bit_position++;
    } else if ( (int)Alarms[i] == '\x31' ) {
      /* 1 - switch on the relevant bit in the mono mask */
      mono_mask[bit_position/8] = mono_mask[bit_position/8] + (int)pow(2, (bit_position%8));
    } else if ( (int)Alarms[i] == '\x30' ) {
      /* 0 - don't change anything */
    }

    bit_position++;
    i++;
  }

  sprintf(temp_message_space, "Mask Length: %d", strlen(Alarms));
  log_message(5, temp_message_space);

  sprintf(temp_message_space, "Stereo Mask: 0x  %.2x  %.2x  %.2x  %.2x\nMono Mask  : 0x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", stereo_mask[0], stereo_mask[1], stereo_mask[2], stereo_mask[3], mono_mask[0], mono_mask[1], mono_mask[2], mono_mask[3], mono_mask[4], mono_mask[5], mono_mask[6], mono_mask[7]);
  log_message(5, temp_message_space);

  return 0;
}

int email(char *message) {
  // Fork off the process here
  pid_t pid_email;

  log_message(3, "About to fork off email process");
  pid_email = fork();
  if ( pid_email < 0 ) {
    // Fork failed - parent only
    log_message(2, "Email fork failed"); // ## 3 ##
    return 1;
  } else if ( pid_email == 0 ) {
    // Fork succeeded - child
    log_message(3, "Email fork succeeded");
    char email_header[1024], email_body[150], time_holder[20];
    char input_message[1024], local_Notify[1024];
    char mem_counter = 0;

    strcpy(input_message, message);
    strcpy(local_Notify, Notify);

    // Open a TCP connection to the SMTP Host //
    int tcp_sock, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    portno = 25;
    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);

    if ( tcp_sock < 0 ) {
      log_message(0, "Eror opening SMTP TCP socket - exiting");
      exit(EXIT_FAILURE);
    }

    server = gethostbyname(SMTP_Host);
    if ( server == NULL ) {
      log_message(0, "SMTP DNS lookup failure - exiting");
      exit(EXIT_FAILURE);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
      (char *)&serv_addr.sin_addr.s_addr,
      server->h_length);

    serv_addr.sin_port = htons(portno);
    if ( connect(tcp_sock, &serv_addr, sizeof(serv_addr)) < 0 ) {
      log_message(0, "Error connecting to SMTP Host port 25 - exiting");
      exit(EXIT_FAILURE);
    }
    log_message(3, "Connected to SMTP Host");

    // Welcome message 
    bzero(temp_message_space, 1024);
    read(tcp_sock, temp_message_space, 1024);
    log_message(5, temp_message_space);

    // SMTP Header //
    n = write(tcp_sock, "HELO localhost\r\nMAIL FROM: root@localhost\r\n", strlen("HELO localhost\r\nMAIL FROM: root@localhost\r\n"));
    if ( n < 0 ) {
      log_message(0, "Unable to identify self to SMTP Host - exiting");
      exit(EXIT_FAILURE);
    } // next up: recipient list //
    // Response to HELO //
    bzero(temp_message_space, 1024);
    read(tcp_sock, temp_message_space, 1024);
    log_message(5, temp_message_space);

    // Response to MAIL FROM //
    bzero(temp_message_space, 1024);
    read(tcp_sock, temp_message_space, 1024);
    log_message(5, temp_message_space);

    char *rcpt_to = NULL;
    rcpt_to = strtok(local_Notify, ",\n");
    while (rcpt_to != NULL ) {
      bzero(temp_message_space, 1024);
      sprintf(temp_message_space, "RCPT TO: <%s>\r\n", rcpt_to);
//printf("%s", temp_message_space);
      n = write(tcp_sock, temp_message_space, strlen(temp_message_space));
      if ( n < 0 ) {
        log_message(0, "Unable to add email recipient - exiting");
        exit(EXIT_FAILURE);
      }
      // Response to RCPT TO //
      bzero(temp_message_space, 1024);
      read(tcp_sock, temp_message_space, 1024);
      log_message(5, temp_message_space);

      rcpt_to = strtok(NULL, ",\n");
    } // and next the DATA command //
    n = write(tcp_sock, "DATA\r\n", strlen("DATA\r\n"));
    if ( n < 0 ) {
      log_message(0, "Unable to send DATA command to SMTP Host - exiting");
      exit(EXIT_FAILURE);
    }
    // Response to DATA //
    bzero(temp_message_space, 1024);
    read(tcp_sock, temp_message_space, 1024);
    log_message(5, temp_message_space);

    sleep(1);
    // email header //
    time_t now;
    struct tm *l_time;

    time(&now);
    l_time = localtime(&now);
    strftime(time_holder, sizeof(time_holder), "%b-%d@%H-%M-%S", l_time);
    sprintf(email_header, "From: <%s>\r\nSubject: %s\r\nContent-Type: text/plain; charset=ISO-8859-1\r\n\r\n", time_holder, input_message);
    n = write(tcp_sock, email_header, strlen(email_header));
    if ( n < 0 ) {
      log_message(0, "Unable to send email header to SMTP Host - exiting");
      exit(EXIT_FAILURE);
    }

    // email body //
    // Reset the email message to CRLF's - stops quoted-printable characters in SMS //
    while ( mem_counter <= 75 ) {
      memcpy(&email_body[0+(2*mem_counter)], "\r\n", 2);
      mem_counter++;
    }
    memcpy(&email_body[144], "\r\n.\r\n\0", 6);
    //memcpy(email_body, input_message, strlen(input_message));
    n = write(tcp_sock, email_body, strlen(email_body));
    if ( n < 0 ) {
      log_message(0, "Unable to send email body to SMTP Host");
      exit(EXIT_FAILURE);
    }

    // Response to End of email //
    bzero(temp_message_space, 1024);
    read(tcp_sock, temp_message_space, 1024);
    log_message(5, temp_message_space);

    n = write(tcp_sock, "QUIT\r\n", strlen("QUIT\r\n"));
    if ( n < 0 ) {
      log_message(0, "Unable to send QUIT command to SMTP Host - exiting");
      exit(EXIT_FAILURE);
    }

    // Response to QUIT //
    bzero(temp_message_space, 1024);
    read(tcp_sock, temp_message_space, 1024);
    log_message(5, temp_message_space);

    // Tidy up the TCP connection because it's work is done //
    close(tcp_sock);

    exit(EXIT_SUCCESS);
  }

  // The child has been forked off and will signal when it exits.
  // The parent will return now.
  return 0; 
}

int notify(int i, int state, char mors) {
  int j;

  sprintf(temp_message_space, "notify(): i: %d\tstate: %d\tmors: %c", i, state, mors);
  log_message(3, temp_message_space);

  if ((int)mors == 'm') {
    /* MONO */
    if ( state == -1 ) {
      /* Audio loss notification */
      for ( j = 0; j < 8; j++ ) {
        if ( mono_delay_counter[i][j] == (Delay * DataRate) ) {
          sprintf(temp_message_space, "%s is off the air", chan_line[(i*8)+j+((i*8)+j+2)/2] );
          log_message(0, temp_message_space);
          email(temp_message_space);
        }
      }
    } else {
      /* It must be a return of audio notice */
      sprintf(temp_message_space, "%s restored after %dm %ds of silence", chan_line[(i*8)+state+((i*8)+state+2)/2], ((mono_delay_counter[i][state]/DataRate)+5)/60, ((mono_delay_counter[i][state]/DataRate)+5)%60 );
      log_message(0, temp_message_space);
      email(temp_message_space);
    }
  }

  if ((int)mors == 's') {
    /* STEREO */
    if ( state == -1 ) {
      /* Audio loss notification */
      for ( j = 0; j < 8; j++ ) {
        if ( stereo_delay_counter[i][j] == (Delay * DataRate) ) {
          sprintf(temp_message_space, "%s is off the air", chan_line[(i*24)+(j*3)] );
          log_message(0, temp_message_space);
          email(temp_message_space);
        }
      }
    } else {
      /* It must be a return of audio notice */
      sprintf(temp_message_space, "%s restored after %dm %ds of silence", chan_line[(i*24)+(state*3)], ((stereo_delay_counter[i][state]/DataRate)+5)/60, ((stereo_delay_counter[i][state]/DataRate)+5)%60 );
      log_message(0, temp_message_space);
      email(temp_message_space);
    }
  }

  return 0;
}

int delay_ok(int i, unsigned char alarm_result, char mors) {
  int bit_position = 128;
  int j = 7;
  int ret = 0;

sprintf(temp_message_space, "delay_ok(): i: %d\talarm_result: %.2x\tmors: %c", i, alarm_result, mors);
log_message(4, temp_message_space);

  if ((int)mors == 'm') {
    while ( bit_position >= 1 ) {
      if ( alarm_result >= bit_position ) {
        /* We have found a high bit, so a timeout increment is in order */
        alarm_result = alarm_result - bit_position;
        mono_delay_counter[i][j]++;
      } else {
        /* We will want to check if [i][j] was above the notify threshold
           If it was we need to notify that the condition has returned to normal */
        if ( mono_delay_counter[i][j] >= ( Delay * DataRate ) ) notify(i, j, 'm');
        mono_delay_counter[i][j] = 0;
      }
      /* Have we reached the notify threshold */
      if ( mono_delay_counter[i][j] == ( Delay * DataRate ) ) ret = 1;
      --j;
      bit_position = bit_position >> 1;
    }
  }

  if ((int)mors == 's') {
    while ( bit_position >= 1 ) {
      if ( alarm_result >= bit_position ) {
        /* We have found a high bit, so a timeout increment is in order */
        alarm_result = alarm_result - bit_position;
        stereo_delay_counter[i][j]++;
      } else {
        /* We will want to check if [i][j] was above the notify threshold
           If it was we need to notify that the condition has returned to normal */
        if ( stereo_delay_counter[i][j] >= ( Delay * DataRate ) ) notify(i, j, 's');
        stereo_delay_counter[i][j] = 0;
      }
      /* Have we reached the notify threshold */
      if ( stereo_delay_counter[i][j] == ( Delay * DataRate ) ) ret = 1;
      --j;
      bit_position = bit_position >> 1;
    }
  }

  /* A silence has been present for the required period */
  if ( ret == 1 ) return 0;
  /* Otherwise return 1 */
  return 1;
}

int check_alarms(void) {
  /* Byte 93 is the first alarm byte. 95 is the second, ... */
  int alarm_byte_offset, j;
  unsigned char alarm_result;

log_message(4, "check_alarms()");

  /* MONO */
  for ( alarm_byte_offset = 0; alarm_byte_offset < 8; alarm_byte_offset++ ) {
    alarm_result = alarm_buffer[93+(alarm_byte_offset*2)] & mono_mask[alarm_byte_offset];

    sprintf(temp_message_space, "MAlarm: 0x%.2x", alarm_buffer[93+(alarm_byte_offset*2)]);
    log_message(6, temp_message_space);

    sprintf(temp_message_space, "MoMask: 0x%.2x", mono_mask[alarm_byte_offset]);
    log_message(6, temp_message_space);

    sprintf(temp_message_space, "Result: 0x%.2x\n", alarm_result);
    log_message(6, temp_message_space);

    /* This code runs a function that takes about 4% of CPU.
       It is pretty redundant most of the time, but if excluded
       by adding an extra test "alarm_result > 0 & " at the beginning,
       the delay_counter will not be reset and a restoration notice
       will not be sent. */
    if ( delay_ok(alarm_byte_offset, alarm_result, 'm') == 0 ) notify(alarm_byte_offset, -1, 'm');
  }

  /* STEREO */
  unsigned char stereo_alarms[4];
  bzero(&stereo_alarms[0], 4);

  // Make one bit from two legs of a stereo alarm
  for ( alarm_byte_offset = 0; alarm_byte_offset < 8; alarm_byte_offset++ ) {
    for ( j = 8; j > 0; j=j>>1 ) {
      if ( alarm_buffer[93+(alarm_byte_offset*2)] >= 192 ) {
        stereo_alarms[alarm_byte_offset/2] = stereo_alarms[alarm_byte_offset/2] + j + j*((alarm_byte_offset%2)*15);
      }
      alarm_buffer[93+(alarm_byte_offset*2)] = alarm_buffer[93+(alarm_byte_offset*2)] << 2;
    }

  }

  for ( alarm_byte_offset = 0; alarm_byte_offset < 4; alarm_byte_offset++ ) {
    alarm_result = stereo_alarms[alarm_byte_offset] & stereo_mask[alarm_byte_offset];

    sprintf(temp_message_space, "SAlarm: 0x%.2x", stereo_alarms[alarm_byte_offset]);
    log_message(6, temp_message_space);

    sprintf(temp_message_space, "StMask: 0x%.2x", stereo_mask[alarm_byte_offset]);
    log_message(6, temp_message_space);

    sprintf(temp_message_space, "Result: 0x%.2x\n", alarm_result);
    log_message(6, temp_message_space);

    /* This code runs a function that takes about 4% of CPU.
       It is pretty redundant most of the time, but if excluded
       by adding an extra test "alarm_result > 0 & " at the beginning,
       the delay_counter will not be reset and a restoration notice
       will not be sent. */
    if ( delay_ok(alarm_byte_offset, alarm_result, 's') == 0 ) notify(alarm_byte_offset, -1, 's');

  }

  return 0;
}

int main(int argc, char **argv) {
  /* Do we want to query the data streams */
  int query = 0;
  int halt = 0;

  /* Check command line arguments */
  int i = 1;
  *argv++; /* Skip the program name */
  while (i < argc) {
    if ( !strcmp(*argv, "-c") ) {
      i++; /* Increment the while counter */
      strcpy(conf_file, *(++argv));
    } else if ( !strcmp(*argv, "-q") ) {
      /* We want to query datastreams at a more appropriate point */
      query = 1;
      sprintf(Logfile, "/dev/stdout");
      log_message(0, "Chromatec SMS Alarm System - Query Mode");
    } else if ( !strcmp(*argv, "-h") ) {
      /* We want to halt all datastreams at a more appropriate point */
      halt = 1;
      sprintf(Logfile, "/dev/stdout");
      log_message(0, "Chromatec SMS Alarm System - Halting Data Streams");
    } else if ( !strcmp(*argv, "-v") ) {
      /* Increase the verbosity of logging */
      verbosity++;
    }
    i++;
    *argv++;
  }

  log_message(3, "Reading Configuration File");
  if ( read_config(conf_file) != 0 ) log_message(2, "There was a problem loading the config file");

  log_message(3, "Reading Channel-Names File");
  if ( read_channelfile() != 0 ) log_message(2, "There was a problem loading the channel names");

  if ( query == 1 ) {
    /* We have been asked to query the Chromatec's current Data Streams */
    printf("Data Stream Handle Key:\n");
    printf("Normal Stream\tPresistent\tPersistent After Reboot\n");
    printf("1 to 65485\t65496 to 65515\t65516 to 65535\n");
    printf("\nData Streams:\n");
    if ( queryds() == 0 ) {
      printf("\n");
      exit(EXIT_SUCCESS);
    } else {
      printf("There was a problem querying the Chromatec host\n");
      exit(EXIT_FAILURE);
    }
  }

  if ( halt == 1 ) {
    if ( haltds() == 0 ) {
      exit(EXIT_SUCCESS);
    } else {
      printf("There was a problem halting the Chromatec datastreams\n");
      exit(EXIT_FAILURE);
    }
  }

  log_message(0, "Chromatec SMS Alarm System - Starting");

  log_message(3, "Building Alarm Request Masks");
  if ( build_masks() != 0 ) {
    log_message(0, "Failed to build Masks - exiting");
    exit(EXIT_FAILURE);
  }

  log_message(3, "Starting Chromatec Data Stream");
  if ( startds() != 0 ) {
    log_message(0, "Failed to start Data Stream - exiting");
    exit(EXIT_FAILURE);
  }

  // Fork off the process here
  pid_t pid, sid;

  pid = fork();
  if ( pid < 0 ) {
    // Fork failed - parent only
    exit(EXIT_FAILURE);
  } else if ( pid > 0 ) {
    // Fork succeeded - parent only
    exit(EXIT_SUCCESS);
  } // Child only from here on because parent exits above
  umask(117);

  sid = setsid();
  if ( sid < 0 ) {
    log_message(0, "Daemon unable to set sid - exiting");
    exit(EXIT_FAILURE);
  }

  if ( ( chdir("/usr/bin/") ) < 0 ) {
    log_message(0, "Daemon cannot change directory - exiting");
    exit(EXIT_FAILURE);
  }

  /* close all descriptors */
  for ( i = getdtablesize(); i >= 0; --i ) close(i);
  /* and reopen important ones */
  i = open("/dev/null", O_RDWR); /* open stdin */
  dup(i); /* stdout */
  dup(i); /* stderr */

  signal(SIGUSR1, log_pcount_value);
  signal(SIGHUP, logrotate_ack);
  signal(SIGTERM, exit_gracefully);
  signal(SIGCHLD, child_is_done);

  while (1) {
    if ( get_alarm_packet() != 0 ) {
      log_message(0, "Problem getting alarm data from Chromatec - exiting");
      exit(EXIT_FAILURE);
    }

/* Alarm Simulator */
//if (pcount > (10*DataRate) && pcount < (30*DataRate)) alarm_buffer[93] = '\xc0'; /* 01100011 */
//if (pcount > (12*DataRate) && pcount < (24*DataRate)) alarm_buffer[101] = 8; /* 00010000 */

    if ( check_alarms() != 0 ) {
      log_message(0, "Problem testing alarm status - exiting");
      exit(EXIT_FAILURE);
    }
  }

  // We'll never get to here
  exit(EXIT_SUCCESS);
}
