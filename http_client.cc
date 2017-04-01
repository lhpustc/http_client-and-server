//
//  main.c
//  webclient_server
//
//  Created by Haipeng LU on 1/11/17.
//  Copyright © 2017 Haipeng LU. All rights reserved.
//

#include "minet_socket.h"
#include "stdlib.h"
#include "ctype.h"
#include "string.h"

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    int datalen = 0;
    bool ok = true;
    struct sockaddr_in sa;
    FILE * wheretoprint = stdout;
    struct hostent * site = NULL;
    char * req = NULL;

    char buf[BUFSIZE + 1];
    char * bptr = NULL;
    char * bptr2 = NULL;
    char * endheaders = NULL;

    struct timeval timeout;
    fd_set set;

    /*parse args */
    if (argc != 5) {
        fprintf(stderr, "usage: http_client k|u server port path\n");
        exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];



    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') {
        minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') {
        minet_init(MINET_USER);
    } else {
        fprintf(stderr, "First argument must be k or u\n");
        exit(-1);
    }

    /* create socket */
    sock = minet_socket(SOCK_STREAM);
    if (sock == -1){
        fprintf(stderr,"Failed to create socket.");
        exit(-1);
    }
    //printf("create socket: %d\n",sock);

    // Do DNS lookup
    /* Hint: use gethostbyname() */
    site = gethostbyname(server_name);
    if (site == NULL){
        fprintf(stderr,"Failed to get hostname.\n");
        minet_close(sock);
        exit(-1);
    }

    /* set address */
    memset(&sa,0,sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(server_port);
    sa.sin_addr.s_addr = *(unsigned long *)site->h_addr_list[0];
    /* connect socket */
    if (minet_connect(sock, &sa)){
        fprintf(stderr,"Failed minet_connect().");
        minet_close(sock);
        exit(-1);
    }
    //printf("connect succesfully\n");
    /* send request */
    req = (char *)malloc(100);
    sprintf(req, "GET %s HTTP/1.0\n\n",server_path);
    minet_write(sock, req, strlen(req));
    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */
    FD_ZERO(&set);
    FD_SET(sock,&set);
    rc = minet_select(sock+1, &set, NULL, NULL, NULL);

    if (rc < 0){
        fprintf(stderr, "Failed minet_select()");
        exit(-1);
    }
    //printf("rc of select(): %d\n",rc);
    //if (rc > 0) printf("minet_select() successful\n");
    /* first read loop -- read headers */
    if (FD_ISSET(sock,&set)){
        //printf("ready to read\n");
        rc = minet_read(sock, buf, BUFSIZE);
        /* examine return code */
        //Skip "HTTP/1.0"
        //remove the '\0'
        // Normal reply has return code 200
        /* print first part of response */
        char code[4];
        bptr=buf;
        while (bptr[0] != ' ') bptr++;
        strncpy(code,bptr+1,3);
        code[3]='\0';
        fprintf(wheretoprint,"Return Code: %s\n",code);
        bptr+=6;
        while(bptr[-1] != '\n') bptr++;
        while (!(bptr[0]=='\r' && bptr[1]=='\n' && bptr[2]=='\r' && bptr[3]=='\n')) {
          fprintf(wheretoprint, "%c", bptr[0]);
          bptr++;
        }
        fprintf(wheretoprint, "\n\nHTML Page\n");
        fprintf(wheretoprint,"%s",bptr+4);
        /* second read loop -- print out the rest of the response */
        while((datalen = minet_read(sock, buf, BUFSIZE)) > 0){
            buf[datalen]='\0';
            fprintf(wheretoprint, "%s", buf);
        }
    }

    /*close socket and deinitialize */
    ok = minet_close(sock);
    ok = minet_deinit();

    if (ok) {
        return 0;
    } else {
        return -1;
    }
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
        totalwritten += rc;
    }

    if (rc < 0) {
        return -1;
    } else {
        return totalwritten;
    }
}
