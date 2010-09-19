#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "spellcast.h"

int
main(int argc, char *argv[])
{
  spellcast_server* server_info;
  //TODO: argument checking

  server_info = init_variables(argc, argv);
  if (server_info == NULL){
    dispose_server(server_info);
    return 1;
  }

  init_server(server_info);
  run(server_info);

  dispose_server(server_info);
  return 0;
}

static spellcast_server*
init_variables(int args, char *argv[])
{
  spellcast_server *srv;
  srv = (spellcast_server*) malloc(sizeof(spellcast_server));
  
  if (!srv){
    P_ERROR("malloc gave NULL (intializing server)");
    return NULL; 
  }

  srv->client_port = "8000";
  srv->source_port = "8001";

  // server metadata
  srv->server_metadata = (server_meta*) malloc(sizeof(server_meta));
  if (!srv->server_metadata){
    P_ERROR("malloc gave NULL (initializing server : server metadata)");
    return NULL;
  }

  srv->server_metadata->metaint = 16000;
  srv->server_metadata->notice = "You are connected to a dummy spellcast server 0.1";

  srv->server_metadata->server_data = (stream_meta*) malloc(sizeof(stream_meta));
  if (!srv->server_metadata->server_data){
    P_ERROR("malloc gave NULL (initializing server : stream data)");
    return NULL;
  }

  srv->server_metadata->server_data->name = "SpellCast dummy server";
  srv->server_metadata->server_data->url = "http://github.com/Tumas/labs/V_semester/networks";
  srv->server_metadata->server_data->mime_type = "audio/mpeg";
  srv->server_metadata->server_data->pub = 0; /* private server */
  srv->connected_sources = 0;
  srv->connected_clients = 0;

  return srv;
}


static int
init_server(spellcast_server *srv)
{
  memset(&srv->hints, 0, sizeof srv->hints);
  srv->hints.ai_family = AF_UNSPEC;
  srv->hints.ai_socktype = SOCK_STREAM;
  srv->hints.ai_flags = AI_PASSIVE;

  srv->src_sock = create_access_point(&srv->hints, &srv->srv_src_addrinfo, srv->source_port);
  srv->cl_sock = create_access_point(&srv->hints, &srv->srv_cl_addrinfo, srv->client_port);

  if (srv->src_sock == -1 || srv->cl_sock == -1){
    close(srv->src_sock);
    close(srv->cl_sock);

    P_ERROR("Server could not bind");
    return -1;
  }

  if (listen(srv->src_sock, SOURCEBACKLOG) == -1 || listen(srv->cl_sock, CLIENTBACKLOG) == -1){
    perror("Could not listen");
    return -1;
  }

  FD_ZERO(&srv->read_socks);
  FD_SET(srv->src_sock, &srv->read_socks);
  FD_SET(srv->cl_sock, &srv->read_socks);
  srv->latest_sock = srv->cl_sock;
  return 0;
}

int
create_access_point(struct addrinfo* hints, struct addrinfo** serv_info, char *port)
{
  int status, sock, reuse_flag = 1;
  struct addrinfo *temp;

  if ((status = getaddrinfo(NULL, port, hints, serv_info)) != 0){
    P_ERROR(gai_strerror(status));
    return -1;
  }

  /* 
    Now, it's possible to get some addresses that don't work for one reason or another, so what the Linux man page does is loops through the list doing a call to socket() and connect() (or bind() if you're setting up a server with the AI_PASSIVE flag) until it succeeds.
   */
  for (temp = *serv_info; temp != NULL; temp = temp->ai_next){
    // create a socket
    sock = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
    if (sock < 0){ 
      continue;
    }

    // set reuseaddr
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_flag, sizeof(int));

    // bind socket
    if (bind(sock, temp->ai_addr, temp->ai_addrlen) < 0){
      close(sock);
      continue;
    }

    return sock;
  }

  return -1; //could not bind
}

static void 
dispose_server(spellcast_server *srv)
{
  free(srv->server_metadata->server_data);
  free(srv->server_metadata);

  freeaddrinfo(srv->srv_src_addrinfo);
  freeaddrinfo(srv->srv_cl_addrinfo);
  free(srv);
}


static int 
run(spellcast_server *srv)
{
  int i;
  fd_set temp_read;

  while (1){
    temp_read = srv->read_socks;
    if (select(srv->latest_sock+1, &temp_read, NULL, NULL, NULL) == -1){
      perror("Select");
      return -1;
    }

    for(i = 0; i < srv->latest_sock; i++){
      if (FD_ISSET(i, &temp_read)){
        
        if (i == srv->src_sock){
          accept_source(srv);
        }
        else if (i == srv->cl_sock){
          accept_client(srv);
        }
        else {
          // we have src broadcasting
          // src or client disconnecting
        }

      } // FD_ISSET
    }
  } // while(1)
}

static int 
accept_source(spellcast_server *srv)
{
  struct sockaddr_storage remote_addr;
  socklen_t addrlen = sizeof remote_addr;
  source_meta *new_src;
  int new_src_sock, len;

  // TODO: shows notification but doesn't notify source that's trying to connect
  if (srv->connected_sources == MAX_SOURCES){
    P_ERROR("max number of clients already reached");
    return -1;
  }

  if ((new_src_sock = accept(srv->src_sock, (struct sockaddr*) &remote_addr, &addrlen)) == -1){
    perror("Source accept");
    return -1;
  }

  // TODO: print some useful info
  printf("New connection from: \n");

  /**  READING DATA that could actually block **/

  // TODO: this could actually block
  if ((len = recv(new_src_sock, srv->buffer, sizeof srv->buffer - 1, 0)) <= 0){
    if (len == 0){
      return -1;
    }
    else {
      perror("Initial receive:");
    }
  }

  srv->buffer[len] = "\0";
  parse_source_details(srv->buffer, len, &new_src);

  // TODO: shows notification but doesn't notify source that's trying to connect
  // if (check_mountpoint(new_src) == -1) { P_ERROR("mountpoint already taken"); return -1; }

  // register_source:; 
  //  update connected_clients size
  //  add to list 

  /** END OF POSSIBLE BLOCKING **/

  // update top sockets : mountpoint is ok, limit is ok, source created
  FD_SET(new_src_sock, &srv->read_socks);
  if (new_src_sock > srv->latest_sock){
    srv->latest_sock = new_src_sock;
  }

  // send ok header? 
  return 0;
}

static void
parse_source_details(char *buffer, int width, source_meta *src)
{
  printf(" %s\n %d\n", buffer, width);
}

static int 
accept_client(spellcast_server* srv)
{
  struct sockaddr_storage remote_addr;
  socklen_t addrlen = sizeof remote_addr;
  int new_cl_sock;

  if ((new_cl_sock = accept(srv->cl_sock, (struct sockaddr*) &remote_addr, &addrlen)) == -1){
    perror("Client accept");
    return -1;
  }

  FD_SET(new_cl_sock, &srv->read_socks);
  if (new_cl_sock > srv->latest_sock){
    srv->latest_sock = new_cl_sock;
  }

  // TODO
  printf("New Client connection from: \n");

  return 0;
}

/*
static void
broadcast()
{
}
*/
