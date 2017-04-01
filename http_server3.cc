#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>


#define FILENAMESIZE 100
#define BUFSIZE 1024

typedef enum \
{NEW,READING_HEADERS,WRITING_RESPONSE,READING_FILE,WRITING_FILE,CLOSED} states;

typedef struct connection_s connection;
typedef struct connection_list_s connection_list;

struct connection_s
{
    int sock;
    int fd;
    char filename[FILENAMESIZE+1];
    char buf[BUFSIZE+1];
    char *endheaders;
    bool ok;
    long filelen;
    states state;
    int headers_read,response_written,file_read,file_written;

    connection *next;
};

struct connection_list_s
{
    connection *first,*last;
};

void add_connection(int,connection_list *);
void insert_connection(int,connection_list *);
void init_connection(connection *con);


int writenbytes(int,char *,int);
int readnbytes(int,char *,int);
void read_headers(connection *);
void write_response(connection *);
void read_file(connection *);
void write_file(connection *);

int main(int argc,char *argv[])
{
    printf("This is main() function\n");

    int server_port;
    int sock,sock2;
    struct sockaddr_in sa,sa2;
    int rc;
    fd_set readlist,writelist;
    connection_list connections;
    connection *i;
    int maxfd;

    /* parse command line args */
    if (argc != 3)
    {
        fprintf(stderr, "usage: http_server3 k|u port\n");
        exit(-1);
    }
    server_port = atoi(argv[2]);
    if (server_port < 1500)
    {
        fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
        exit(-1);
    }

    /* initialize and make socket */
    if(toupper(*(argv[1])) == 'K'){
        minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') {
        minet_init(MINET_USER);
    } else {
        minet_perror("First argument should be k or u\n");
        exit(-1);
    }

    sock = minet_socket(SOCK_STREAM);
    if (sock == -1) {
        minet_perror("create socket!\n");
        exit(-1);
    }

    /* set server address*/
    memset(&sa, 0, sizeof(sa));
    sa.sin_port = htons(server_port);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    /* bind listening socket */
    rc = minet_bind(sock, &sa);
    if (rc < 0) {
        minet_perror("bind listening socket!\n");
        exit(-1);
    }

    /* start listening */
    rc = minet_listen(sock, 10);
    if (rc < 0){
        minet_perror("start listening\n");
        exit(-1);
    }

    maxfd = sock;
    //FD_ZERO(&readlist);
    //FD_ZERO(&writelist);
    //init_connection(i);

    //FD_SET(sock, &readlist);

    connections.first = NULL;
    connections.last = NULL;
    /* connection handling loop */
    while(1)
    {
        /* create read and write lists */
        printf("New while loop\n");

        printf("clear readlist & writelist\n");
        FD_ZERO(&readlist);
        FD_ZERO(&writelist);

        printf("create new readlist and writelist\n");
        i = connections.first;
        while (i != NULL){
            if (i->state == NEW || i->state == READING_HEADERS){
                FD_SET(i->sock, &readlist);
                maxfd = (maxfd > i->sock) ? maxfd:i->sock;
            }
            else if (i->state == READING_FILE){
                FD_SET(i->fd, &readlist);
                maxfd = (maxfd > i->fd) ? maxfd:i->fd;
            }
            else if (i->state == WRITING_RESPONSE || i->state == WRITING_FILE){
                FD_SET(i->sock, &writelist);
                maxfd = (maxfd > i->sock) ? maxfd:i->sock;
            }
            i = i->next;
        }

        printf("add server socket\n");
        FD_SET(sock, &readlist);
        //fcntl(sock, F_SETFL, O_NONBLOCK);
        maxfd = (maxfd > sock) ? maxfd:sock;

        /* do a select */
        printf("perform select\n");
        rc = minet_select(maxfd+1, &readlist, &writelist, NULL, NULL);
        if (rc < 0){
            minet_perror("minet_select\n");
            exit(-1);
        }

        /* process sockets that are ready */
        printf("process sockets that are ready\n");
        for(int j=0;j<=maxfd;j++){
            // process readlist
            if (FD_ISSET(j, &readlist)){
                // server_socketet: accept socket
                if (j == sock){
                    sock2 = minet_accept(sock, &sa2);
                    if (sock2 < 0){
                        printf("No new connections to accept...\n");
                        continue;
                    }
                    // add open connections
                    fcntl(sock2,F_SETFL,O_NONBLOCK);
                    insert_connection(sock2, &connections);
                }
                // client_socket or file: handle_read
                else {
                    i = connections.first;
                    while(i) {
                        // client_socket read
                        if (j == i->sock) {
                            i->state = READING_HEADERS;
                            init_connection(i);
                            read_headers(i);
                            break;
                        }
                        // file read
                        else if (j == i->fd){
                            read_file(i);
                            break;
                        }
                        i = i->next;
                    }
                }
            }
            // process writelist
            else if (FD_ISSET(j,&writelist)){
                i = connections.first;
                while(i) {
                    if (j == i->sock) {
                        if (i->state == WRITING_RESPONSE)
                            write_response(i);
                        else
                            write_file(i);
                        break;
                    }
                    i = i->next;
                }
            }
            printf("End process sockets\n");
        }
        printf("End of while(1)\n");
    }
}


void read_headers(connection *con)
{
  printf("Tread_headers())\n");
    int rc;
    int fd;
    /* first read loop -- get request and headers*/
    printf("read_headers: first read loop\n");
    printf("headers_read: %d\n", con->headers_read);
    // while (rc = minet_read(con->sock, con->buf+con->headers_read, BUFSIZE-con->headers_read)) > 0){
    //     con->headers_read += rc;
    //     printf("headers_read: %d\n", con->headers_read);
    //     //con->buf[con->headers_read] = '\0';
    //     if ((con->endheaders = strstr(con->buf, "\r\n\r\n")) != NULL){
    //         con->endheaders[4] = '\0';
    //         break;
    //     }
    // }
    //printf("End first read loop\n");

    rc = minet_read(con->sock, con->buf, BUFSIZE);

    if (rc < 0) {
        if (errno == EAGAIN) {
            //con->state = READING_HEADERS;
            printf("errno == EAGAIN\n");
            return;
        }
        minet_perror("read_headers\n");
        con->ok = false;
    }
    else if(rc == 0){
        minet_perror("Invalid request!\n");
        con->ok = false;
    }


    else {
      con->headers_read += rc;
      if ((con->endheaders = strstr(con->buf, "\r\n\r\n")) != NULL){
          con->endheaders[4] = '\0';
      }
        /* parse request to get file name */
        /* Assumption: this is a GET request and filename contains no spaces*/
        char *bptr = strtok(con->buf, " ");
        bptr = strtok(NULL," ");
        if (bptr[0] == '/') bptr++;
        if (bptr == NULL) {
            minet_perror("No filename\n");
            con->ok = false;
        }
        else {
            /* get file name and size, set to non-blocking */
            /* get name */
            if (strlen(bptr) > FILENAMESIZE){
                minet_perror("Filename overflow\n");
                con->ok = false;
            }
            else {
                strcpy(con->filename, bptr);
                printf("Openning file %s...\n", con->filename);
                /* try opening the file */
                if ((fd = open(con->filename, O_RDONLY)) < 0){
                    minet_perror("open file\n");
                    con->ok = false;
                }
                else {
                    con->fd = fd;
                    /* set to non-blocking, get size */
                    fcntl(con->fd, F_SETFL, O_NONBLOCK);

                    struct stat st;
                    stat(con->filename, &st);
                    con->filelen = st.st_size;
                    con->ok = true;
                }
            }
        }
    }

    con->state = WRITING_RESPONSE;
    printf("End of read_headers -> write_response\n");
    write_response(con);
}

void write_response(connection *con)
{
  printf("write_response()\n");
    int sock2 = con->sock;
    int rc;
    int written = con->response_written;
    char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
    "Content-type: text/plain\r\n"\
    "Content-length: %d \r\n\r\n";
    char ok_response[100];
    char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
    "Content-type: text/html\r\n\r\n"\
    "<html><body bgColor=black text=white>\n"\
    "<h2>404 FILE NOT FOUND</h2>\n"\
    "</body></html>\n";
    /* send response */
    if (con->ok)
    {
        /* send headers */
        sprintf(ok_response, ok_response_f, con->filelen);
        rc = writenbytes(con->sock, ok_response, strlen(ok_response));
        if (rc < 0){
            if (errno == EAGAIN)
                return;
            minet_perror("write ok response\n");
            con->state = CLOSED;
            minet_close(con->sock);
        }
        else {
            con->state = READING_FILE;
            printf("End of write_response -> read_file\n");
            read_file(con);
        }
    }
    else
    {
        sprintf(ok_response, notok_response);
        rc = writenbytes(con->sock, ok_response, strlen(ok_response));
        if (rc < 0){
            if (errno == EAGAIN)
                return;
            minet_perror("write notok response\n");
            con->state = CLOSED;
            minet_close(con->sock);
            return;
        }
        con->state = CLOSED;
        minet_close(con->sock);
    }
    printf("End of write_response\n");
}


void read_file(connection *con)
{
  printf("read_file())\n");
    int rc;

    /* send file */
    printf("read_file -> readnbytes\n");
    rc = readnbytes(con->fd,con->buf,BUFSIZE);
    if (rc < 0)
    {
        if (errno == EAGAIN){
          printf("errno = EAGAIN: return\n");
          return;
        }
        fprintf(stderr,"error reading requested file %s\n",con->filename);
        return;
    }
    else if (rc == 0)
    {
        printf("Nothing to read: close the connection\n");
        con->state = CLOSED;
        minet_close(con->sock);
    }
    else
    {
        con->file_read = rc;
        con->state = WRITING_FILE;
        printf("read_file -> write_file\n");
        write_file(con);
    }
    printf("End of read_file\n");
}

void write_file(connection *con)
{
  printf("write_file())\n");
    int towrite = con->file_read;
    int written = con->file_written;
    int rc = writenbytes(con->sock, con->buf+written, towrite-written);
    if (rc < 0)
    {
        if (errno == EAGAIN){
          printf("errno = EAGAIN: return\n");
          return;
        }

        minet_perror("error writing file ");
        con->state = CLOSED;
        minet_close(con->sock);
        return;
    }
    else
    {
        con->file_written += rc;
        if (con->file_written == towrite)
        {
            con->state = READING_FILE;
            con->file_written = 0;
            printf("write_file -> read_file\n");
            read_file(con);
        }
        else
            printf("shouldn't happen\n");
    }
    printf("End of write_file\n");
}

int readnbytes(int fd,char *buf,int size)
{
  printf("readbytes())\n");
    int rc = 0;
    int totalread = 0;
    while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
        totalread += rc;
    printf("End of readnbytes\n");
    if (rc < 0)
    {
        return -1;
    }
    else
        return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  printf("This is writenbytes function\n");
    int rc = 0;
    int totalwritten =0;
    while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
        totalwritten += rc;

    printf("End of writenbytes\n");
    if (rc < 0)
        return -1;
    else
        return totalwritten;
}


// inserts a connection in place of a closed connection
// if there are no closed connections, appends the connection
// to the end of the list

void insert_connection(int sock,connection_list *con_list)
{
  printf("This is insert_connection function\n");
    connection *i;
    for (i = con_list->first; i != NULL; i = i->next)
    {
        if (i->state == CLOSED)
        {
            i->sock = sock;
            i->state = NEW;
            return;
        }
    }
    add_connection(sock,con_list);
    printf("End of insert_connection\n");
}

void add_connection(int sock,connection_list *con_list)
{
  printf("This is add_connection function\n");
    connection *con = (connection *) malloc(sizeof(connection));
    con->next = NULL;
    con->state = NEW;
    con->sock = sock;
    if (con_list->first == NULL)
        con_list->first = con;
        printf("First socket: %d\n", con->sock);
    if (con_list->last != NULL)
    {
        con_list->last->next = con;
        con_list->last = con;
    }
    else
        con_list->last = con;
    printf("Last socket: %d\n", con_list->last->sock);
    printf("End of add_connection\n");
}

void init_connection(connection *con)
{
  printf("This is init_connection function\n");
    con->headers_read = 0;
    con->response_written = 0;
    con->file_read = 0;
    con->file_written = 0;
    printf("End of init_connection\n");
}
