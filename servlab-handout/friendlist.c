/*
 * friendlist.c - [Starting code for] a web-based friend-graph manager.
 *
 * Based on:
 *  tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *      GET method to serve static and dynamic content.
 *   Tiny Web server
 *   Dave O'Hallaron
 *   Carnegie Mellon University
 */
#include "csapp.h"
#include "dictionary.h"
#include "more_string.h"

static void doit(int fd);
static dictionary_t *read_requesthdrs(rio_t *rp);
static void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *d);
static void clienterror(int fd, char *cause, char *errnum, 
                        char *shortmsg, char *longmsg);
static void print_stringdictionary(dictionary_t *d);
static void befriend(int fd, dictionary_t *query);
static void fetchFriends(int fd, dictionary_t *query);
static void unfriend(int fd, dictionary_t *query);
static void introduce(int fd, dictionary_t *query);
static void addSingleFriend(char* name1, char* name2);
static void removeSingleFriend(char* name1, char* name2);
char* getUsersFriends(char* user);
pthread_mutex_t lock;
static int sendRequest(int clientFD, char** response, char* getRequest);

char* hostPort;
static dictionary_t *friends;

int main(int argc, char **argv) {

  //added initializaitons:
  pthread_mutex_init(&lock, NULL);
  friends = make_dictionary(COMPARE_CASE_INSENS, free);

  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  hostPort = argv[1];
  
  listenfd = Open_listenfd(argv[1]);

  /* Don't kill the server if there's an error, because
     we want to survive errors due to a client. But we
     do want to report errors. */
  exit_on_error(0);

  /* Also, don't stop on broken connections: */
  Signal(SIGPIPE, SIG_IGN);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    if (connfd >= 0) {
      Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
      printf("Accepted connection from (%s, %s)\n", hostname, port);
      //lets do some scary stuff and add concurency...
      pthread_t threadID;
      int *threadedConnFD = connfd;
      int err = pthread_create(&threadID, NULL, doit, threadedConnFD);
      if(!err){
        //kill the thread as needed.
        pthread_detach(threadID);
      }
    }
  }
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd) {
  char buf[MAXLINE], *method, *uri, *version;
  rio_t rio;
  dictionary_t *headers, *query;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  if (Rio_readlineb(&rio, buf, MAXLINE) <= 0)
    return;
  printf("%s", buf);
  
  if (!parse_request_line(buf, &method, &uri, &version)) {
    clienterror(fd, method, "400", "Bad Request",
                "Friendlist did not recognize the request");
  } else {
    if (strcasecmp(version, "HTTP/1.0")
        && strcasecmp(version, "HTTP/1.1")) {
      clienterror(fd, version, "501", "Not Implemented",
                  "Friendlist does not implement that version");
    } else if (strcasecmp(method, "GET")
               && strcasecmp(method, "POST")) {
      clienterror(fd, method, "501", "Not Implemented",
                  "Friendlist does not implement that method");
    } else {
      headers = read_requesthdrs(&rio);

      /* Parse all query arguments into a dictionary */
      query = make_dictionary(COMPARE_CASE_SENS, free);
      parse_uriquery(uri, query);
      if (!strcasecmp(method, "POST"))
        read_postquery(&rio, headers, query);

      /* For debugging, print the dictionary */
      print_stringdictionary(query);

      /* You'll want to handle different queries here,
         but the intial implementation always returns
         nothing: */
      if (starts_with("/friends?", uri))
        fetchFriends(fd, query);
      else if (starts_with("/befriend?", uri))
        befriend(fd, query);
      else if (starts_with("/unfriend?", uri))
        unfriend(fd, query);
      else if (starts_with("/introduce?", uri))
        introduce(fd, query);

      /* Clean up */
      free_dictionary(query);
      free_dictionary(headers);
    }

    /* Clean up status line */
    free(method);
    free(uri);
    free(version);
  }
}

/*
 * read_requesthdrs - read HTTP request headers
 */
dictionary_t *read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];
  dictionary_t *d = make_dictionary(COMPARE_CASE_INSENS, free);

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    parse_header_line(buf, d);
  }
  
  return d;
}

void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *dest) {
  char *len_str, *type, *buffer;
  int len;
  
  len_str = dictionary_get(headers, "Content-Length");
  len = (len_str ? atoi(len_str) : 0);

  type = dictionary_get(headers, "Content-Type");
  
  buffer = malloc(len+1);
  Rio_readnb(rp, buffer, len);
  buffer[len] = 0;

  if (!strcasecmp(type, "application/x-www-form-urlencoded")) {
    parse_query(buffer, dest);
  }

}

static char *ok_header(size_t len, const char *content_type) {
  char *len_str, *header;
  
  header = append_strings("HTTP/1.0 200 OK\r\n",
                          "Server: Friendlist Web Server\r\n",
                          "Connection: close\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n",
                          "Content-type: ", content_type, "\r\n\r\n",
                          NULL);
  free(len_str);

  return header;
}

/*
 * serve_request - example request handler
 */
 void befriend(int fd, dictionary_t *query) {
  size_t len;
  char *header,*body;
  char *user = dictionary_get(query, "user");
  char** queryFriends = split_string(dictionary_get(query, "friends"), '\n');
  int i = 0; 
  char *currFriend = queryFriends[i];


  printf("got to the friends while loop. queryFriends[0] is: %s\n", queryFriends[0]);

  while(currFriend){
    addSingleFriend(user, currFriend);
    free(currFriend);
    currFriend = queryFriends[++i];
  }

  printf("finished the while loop\n");
  body = getUsersFriends(user);
  printf("body is: %s\n", body);
  len = strlen(body);

  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, len);

  //  free(body);
}

static void fetchFriends(int fd, dictionary_t *query){
  size_t adjustedLength;
  char *header,*body;
  char *user = dictionary_get(query, "user");

  //this is very similar to befriend, except without the for loop to update the friend list first.
  body = getUsersFriends(user);
  adjustedLength = strlen(body);
  printf("adjustedLength = %lu\n", adjustedLength);


  /* Send response headers to client */
  header = ok_header(adjustedLength, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);
  
  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, adjustedLength);
  free(body);
}

static void unfriend(int fd, dictionary_t *query){
  size_t len;
  char *header,*body;
  char *user = dictionary_get(query, "user");
  char** queryFriends = split_string(dictionary_get(query, "friends"), '\n');
  int i = 0; 
  char *currFriend = queryFriends[i];

  while(currFriend){
    removeSingleFriend(user, currFriend);
    free(currFriend);
    currFriend = queryFriends[++i];
  }

  body = getUsersFriends(user);
  printf("body is now: %s\n", body);
  len = strlen(body);

  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, len);
  free(body);
  free(queryFriends);
}

static void introduce(int fd, dictionary_t *query){
  printf("starting the introduce query. our port is: %s \n", hostPort);
  char *header;
  char *host = dictionary_get(query, "host");
  char *user = dictionary_get(query, "user");
  char *friend = query_encode(dictionary_get(query, "friend"));
  char *port = dictionary_get(query, "port");

  char **splitPort = split_string(port, '\n');
  char* fixedPort = splitPort[0];
  printf("comparing hostPort: %s, and queryPort: %s\n,", hostPort, port);
  if(starts_with(hostPort, port)){
    char* friendsToAdd = getUsersFriends(friend);
    char** friendList = split_string(friendsToAdd, '\n');
    int i = 0;
    char* currFriend = friendList[i];
    while(currFriend){
      addSingleFriend(user, currFriend);
      free(currFriend);
      currFriend = friendList[++i];
    }
    header = ok_header(0, "text/html; charset=utf-8");
    Rio_writen(fd, header, strlen(header));
    printf("Response headers:\n");
    printf("%s", header);
    return;
  }

  int clientFD = Open_clientfd(host, port);

  char *getRequest = "GET /friends?user=";
  getRequest = append_strings(getRequest, friend, " HTTP/1.1\r\n\r\n", NULL);
  char* response;

  printf("request processing... \n request is: %s\nsending request now...\n", getRequest);
  int responseLength = sendRequest(clientFD, &response, getRequest);
  //if we have a real response, loop through and process it.
  printf("request sent. response length is %i\n", responseLength);
  if(responseLength > 0){
    printf("response Len was >0\n response was: %s \n", response);
    char **responseFriends = split_string(response, '\n');
    int i = 0;
    char *currFriend = responseFriends[i];
    int processedBytes = 0;
    printf(currFriend);

    while(processedBytes < responseLength){
      //the extra 1 accounts for the \n char removed by split_string
      processedBytes += 1+strlen(currFriend); 
      printf("processing currFriend: %s", currFriend);
      addSingleFriend(currFriend, user);
      free(currFriend);
      i+=1;
      currFriend = currFriend[i];
    }
    free(responseFriends);
  }

  /* Send response headers to client */
  header = ok_header(0, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);
  free(host);
  free(user);
  free(friend);
  free(port);
}

static void addSingleFriend(char* name1, char* name2){
  
  //dont add duplicates!! this will come up with introduce.
  if(!strcmp(name1, name2)){
    printf("duplicate names found. returning early\n");
    return;
  }

  //check to see if we need to make a dictionary for friend 1
  //if so, create one, then assign values as needed.
  pthread_mutex_lock(&lock); //lock the dictionaries before we try to mess with them
  if(!dictionary_get(friends, name1))
    dictionary_set(friends, name1, make_dictionary(COMPARE_CASE_INSENS, free));
  dictionary_set(dictionary_get(friends, name1), name2, NULL);

  //same as above, but with the second friend.
  if (!dictionary_get(friends, name2))
    dictionary_set(friends, name2, make_dictionary(COMPARE_CASE_INSENS, free));
  dictionary_set(dictionary_get(friends, name2), name1, NULL);
  pthread_mutex_unlock(&lock);

}

static void removeSingleFriend(char* name1, char* name2){
  //we don't need to continue if the user's don't exist...
  pthread_mutex_lock(&lock);
  if(!dictionary_get(friends, name1) || !dictionary_get(friends, name2)){
    printf("names weren't found as dictionaries, returning early..\n");
    pthread_mutex_unlock(&lock);
    return;
  }

  //your friendly neighborhood lock
  //remove name2 from name1's friend list...
  dictionary_t *name1Dict = dictionary_get(friends, name1);
  dictionary_remove(name1Dict, name2);
  //and vice versa.
  dictionary_t *name2Dict = dictionary_get(friends, name2);
  dictionary_remove(name2Dict, name1);

  //remove the dictionaries as needed.
  //this will help get the correct output when using getUsersFriends()
  if(dictionary_count(name1Dict) == 0)
    dictionary_remove(friends,name1);
  if(dictionary_count(name2Dict) == 0)
    dictionary_remove(friends,name2);

  //unlock time!
  pthread_mutex_unlock(&lock);
}

char* getUsersFriends(char* user){
  //get the dictionary containing the friends of the specified user.
  pthread_mutex_lock(&lock);
  dictionary_t *usersDict = dictionary_get(friends,user);
  //if the dictionary is empty or doesn't exist,  there's no sense in going forward.
  if(!usersDict){
    pthread_mutex_unlock(&lock);
    printf("no friends for given user: %s were found, returning early.", user);
    return strdup("");
  }

  //loop through the dictionary
  char* friendsStr = dictionary_key(usersDict,0);
  friendsStr = append_strings(friendsStr, "\n", NULL);
  const char* currFriend;
  int numFriends = dictionary_count(usersDict);
  for(int i = 1; i <numFriends; i++){
    currFriend = dictionary_key(usersDict, i);
    friendsStr = append_strings(friendsStr,currFriend,"\n", NULL);
  }
  pthread_mutex_unlock(&lock);
  return friendsStr;
}

static int sendRequest(int clientFD, char** response, char* getRequest){
  //setup to read in the response info
  rio_t rio;
  char buffer[MAXLINE];
  char *responseStatus;
  int requestSize = strlen(getRequest);

  Rio_writen(clientFD, getRequest, requestSize);
  Rio_readinitb(&rio, clientFD);

  dictionary_t *responseHeaders = make_dictionary(COMPARE_CASE_INSENS, free);
  Rio_readlineb(&rio, buffer, MAXLINE);

  parse_status_line(buffer, NULL, &responseStatus, NULL);

  //read until the end!
  while(strcmp(buffer, "\r\n")){
    Rio_readlineb(&rio, buffer, MAXLINE);
    parse_header_line(buffer, responseHeaders);
  }

  printf("resp headers: ");
  print_stringdictionary(responseHeaders);
  printf("\n");
  //check response status
  size_t responseLength = 0;
  if(!strcasecmp(responseStatus, "200")){
    //convert the length to a string
    responseLength = atoi(dictionary_get(responseHeaders, "Content-length"));
    if(responseLength > 0){
      *response = malloc(responseLength);
      Rio_readnb(&rio, *response, responseLength);
    }
  }
  else{
    printf("whoops... reponse code was: %s", responseStatus);
  }

  return responseLength;
}
/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) {
  size_t len;
  char *header, *body, *len_str;

  body = append_strings("<html><title>Friendlist Error</title>",
                        "<body bgcolor=""ffffff"">\r\n",
                        errnum, " ", shortmsg,
                        "<p>", longmsg, ": ", cause,
                        "<hr><em>Friendlist Server</em>\r\n",
                        NULL);
  len = strlen(body);

  /* Print the HTTP response */
  header = append_strings("HTTP/1.0 ", errnum, " ", shortmsg, "\r\n",
                          "Content-type: text/html; charset=utf-8\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n\r\n",
                          NULL);
  free(len_str);
  
  Rio_writen(fd, header, strlen(header));
  Rio_writen(fd, body, len);

  free(header);
  free(body);
}

static void print_stringdictionary(dictionary_t *d) {
  int i, count;

  count = dictionary_count(d);
  for (i = 0; i < count; i++) {
    printf("%s=%s\n",
           dictionary_key(d, i),
           (const char *)dictionary_value(d, i));
  }
  printf("\n");
}
