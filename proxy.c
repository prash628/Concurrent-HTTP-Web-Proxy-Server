/*
* Description: Web Proxy that handles multiple concurrent requests.
*              It simply forwards the request sent by the client to the
*              server and sends back the reply to the client sent by the
*              server. Also, it only handles GET requests and supports
*              HTTP 1.0( without keep-alive-i.e. only one request and then,
*                        the connection is closed)
* Note: It does not cache requests!
*  Name: Prashant Srinivasan
*  Andrew Id: psriniv1
*/

#include <stdio.h>

#include "csapp.h"


/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
void *clientWorkerThread(void *param);
void doit(int fd);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);

/*
* Reference/ Credits: Adapted from Tiny.c
*
*/
int main(int argc, char **argv)
{
    int listenfd;
    int *connfd;
    /* Reason for using *connfd: This will be malloc'd from heap memory.
     * Else, the same variable, when passed around to multiple threads
     * will cause concurrency problems
     */
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    //Ensure that SIGPIPE is ignored
    Signal(SIGPIPE, SIG_IGN);
    /* Check command line args */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(atoi(argv[1]));//argv[1] has port no
    while (1)
    {
        clientlen = sizeof(clientaddr);
        connfd= (int *)Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        Pthread_create(&tid, NULL, clientWorkerThread, (void *)connfd);
    }
    return 0;
}

void *clientWorkerThread(void *param)
{
    int cliFd = *(int *)(param);
    Pthread_detach(pthread_self());
    // The above ensures that the parent does not wait on this thread

    // Freeing the memory that was malloc'd previously.
    Free(param);

    doit(cliFd);

    Close(cliFd);
    return NULL;
}

/*
 * doit - handles a HTTP request/response transaction
 * Ref: Adapted from tiny.c
 */
/* $begin doit */
void doit(int fd)
{
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char headbuf[MAXLINE], sendrequest[MAXLINE];
    char firstHeaderLine[MAXLINE], restOfHeaderLines[MAXLINE];
    char urlbuf[MAXLINE],host[MAXLINE];
    char *portbuf;char *destpathbuf,*hostx;
    int portNo;
    char response[MAXLINE];
    char requestURIpath[MAXLINE];
    int serverFd;
    rio_t rio,serverrio;
    int sz;

    /* Read request line */
    /* Format is : <RequestMethod> <Request-URI> <HTTP-Version>*/
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;
    sscanf(buf, "%s %s %s", method, url, version);


    if (strcasecmp(method, "GET"))
    {
        clienterror(fd, method, "501", "Not Implemented",
                    "This does not implement this method");
        return;
    }

    /*Parse the request-URI */
    /* Format is : either / or http://<website>:<portno> */
    sscanf(url, "http://%s", urlbuf);

    strcpy(host, urlbuf);
    /* Getting url part after http into urlbuf.
    * This is going to be parsed now
    */
    if ((destpathbuf = strstr(urlbuf, "/")) != NULL)
    {

        strcpy(requestURIpath, destpathbuf);
    }
    else
    {
        strcpy(requestURIpath, "/");//This is because it is NULL
    }
    /* Extract port number*/


    if ((portbuf = strstr(urlbuf, ":")) != NULL)
    {
        portNo=atoi(portbuf);
    }
    else
    {
        //defaultport
        portNo=80;
    }

    /* Few lines below are adapted from void read_requesthdrs(rio_t *rp)
    * The following lines are not abstracted away in another function because
    * --Although, it enhances modularity a trade-off which favors readability
    * --and performance was made because additional parameters should have
    * --been passed to extract more information
    */
    Rio_readlineb(&rio, headbuf, MAXLINE);
    while(strcmp(headbuf, "\r\n"))
    {
        Rio_readlineb(&rio, headbuf, MAXLINE);
        if (!strncmp(headbuf, "Host: ",sizeof("Host: ")))
        {
            strcpy(firstHeaderLine, headbuf);
        }
        /* The following headers inside the if statement are ignored
            *because they have to be swapped*/
        if (strncmp(headbuf, "User-Agent: ",sizeof("User-Agent: "))
            && strncmp(headbuf, "Proxy-Connection: ",sizeof("Proxy-Connection: "))
                && strncmp(headbuf, "Connection: ",sizeof("Connection: "))
            )
        {
            strcpy(restOfHeaderLines, headbuf);
        }
    }
    if(strlen(firstHeaderLine)==0){strcpy(firstHeaderLine,host);}
    hostx=strtok(host,"/");

    //Send request to the server
    serverFd = Open_clientfd(hostx, portNo);
    sprintf(sendrequest, "GET %s HTTP/1.0\r\n", requestURIpath);
    sprintf(sendrequest, "%sHost: %s\r\n", sendrequest, hostx);
    sprintf(sendrequest, "%s%s", sendrequest, user_agent_hdr);
    sprintf(sendrequest, "%sConnection: close\r\n", sendrequest);
    sprintf(sendrequest, "%sProxy-Connection: close\r\n", sendrequest);
    sprintf(sendrequest, "%s%s\r\n", sendrequest, restOfHeaderLines);

     //Read reply and send back to the client
    sz=0;
    Rio_readinitb(&serverrio, serverFd);
    Rio_writen(serverFd, sendrequest, strlen(sendrequest));


     while((sz = Rio_readlineb(&serverrio, response, MAXLINE)) != 0)
    {
        Rio_writen(fd, response, sz);

    }
    /* clear the buffer */
    Close(serverFd);
}
/*
 * clienterror - returns an error message to the client
 * Ref: tiny.c
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>Proxy server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}


