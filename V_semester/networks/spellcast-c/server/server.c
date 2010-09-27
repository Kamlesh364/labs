#include "server.h"
#include "source.h"
#include "client.h"

void
spellcast_print_server_info(const spellcast_server* srv)
{
  fprintf(stdout, "Server: %s\nURL: %s\nServing: %s\n", srv->server_metadata->server_data->name,
      srv->server_metadata->server_data->url, srv->server_metadata->server_data->mime_type);
  fprintf(stdout, "Source port: %s\n", srv->source_port);
  fprintf(stdout, "Client port: %s\n", srv->client_port);
}

spellcast_server* 
spellcast_init_server_variables(const char *s_port, const char *c_port, const server_meta *srv_meta)
{
  spellcast_server *srv = (spellcast_server*) malloc(sizeof(spellcast_server));

  if (!srv){
    P_ERROR("malloc gave NULL (intializing server )");
    return NULL; } 
  srv->client_port = (char*) malloc(strlen(c_port) + 1);
  srv->source_port = (char*) malloc(strlen(s_port) + 1);
  strcpy(srv->client_port, c_port);
  strcpy(srv->source_port, s_port);

  // server metadata
  srv->server_metadata = (server_meta*) malloc(sizeof(server_meta));
  if (!srv->server_metadata){
    P_ERROR("malloc gave NULL (initializing server : server metadata)");
    return NULL;
  }

  srv->server_metadata->metaint = srv_meta->metaint;
  srv->server_metadata->notice = (char*) malloc(strlen(srv_meta->notice) + 1);
  strcpy(srv->server_metadata->notice, srv_meta->notice);

  srv->server_metadata->server_data = (stream_meta*) malloc(sizeof(stream_meta));
  if (!srv->server_metadata->server_data){
    P_ERROR("malloc gave NULL (initializing server : server data)");
    return NULL;
  }

  srv->server_metadata->server_data->name = spellcast_allocate_string(srv_meta->server_data->name);
  srv->server_metadata->server_data->url = spellcast_allocate_string(srv_meta->server_data->url);
  srv->server_metadata->server_data->mime_type = spellcast_allocate_string(srv_meta->server_data->mime_type);

  srv->server_metadata->server_data->pub = srv_meta->server_data->pub; 
  srv->connected_sources = 0;
  srv->connected_clients = 0;

  memset(srv->sources, 0, sizeof(srv->sources));
  memset(srv->clients, 0, sizeof(srv->clients));

  srv->icy_p = spellcast_init_icy_protocol_info();

  return srv;
}

static int 
spellcast_create_access_point(struct addrinfo* hints, struct addrinfo** serv_info, char *port)
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

int
spellcast_init_server(spellcast_server *srv)
{
  memset(&srv->hints, 0, sizeof(srv->hints));
  srv->hints.ai_family = AF_UNSPEC;
  srv->hints.ai_socktype = SOCK_STREAM;
  srv->hints.ai_flags = AI_PASSIVE;

  srv->src_sock = spellcast_create_access_point(&srv->hints, &srv->srv_src_addrinfo, srv->source_port);
  srv->cl_sock = spellcast_create_access_point(&srv->hints, &srv->srv_cl_addrinfo, srv->client_port);

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

  FD_ZERO(&srv->master_read);
  FD_SET(srv->src_sock, &srv->master_read);
  FD_SET(srv->cl_sock, &srv->master_read);
  srv->latest_sock = srv->cl_sock;
  return 0;
}

int 
spellcast_server_run(spellcast_server *srv)
{
  int i, j;
  int tow = 0;
  int total = 0;
  int received_bytes, sent_bytes;
  fd_set temp_read;

  FD_ZERO(&temp_read);

  while (1){
    temp_read = srv->master_read;
    if (select(srv->latest_sock+1, &temp_read, NULL, NULL, NULL) == -1){
      perror("Select");
      return -1;
    }

    for(i = 0; i <= srv->latest_sock; i++){
      if (FD_ISSET(i, &temp_read)){
        
        if (i == srv->src_sock){
          spellcast_accept_source(srv);
        }
        else if (i == srv->cl_sock){
          spellcast_accept_client(srv);
        }
        else {

          source_meta *source = (source_meta*) spellcast_get_source(srv, i);
          if (source){
            if (FD_ISSET(i, &srv->empty_sources)){

              if (spellcast_source_parse_header(srv, source)){
                spellcast_print_source_info(source);

                send_message(source->sock_d, srv->icy_p->ok_message, strlen(srv->icy_p->ok_message));

                FD_CLR(i, &srv->empty_sources);
              }
            }
            else {
              // replace with actual data
              icy_metadata stub = { "U2 - One", NULL };

              if ((received_bytes = recv(source->sock_d, source->buffer, BUFFLEN - tow, 0)) == 0){
                spellcast_disconnect_source(srv, source);
              }
              else 
              {
                // send data to clients
                for (j = 0; srv->connected_clients != 0 && j < MAX_CLIENTS; j++){
                  if (source->clients[j] != NULL){

                    sent_bytes = send_message(source->clients[j]->sock_d, source->buffer, received_bytes);
                    total += sent_bytes;

                    if (total == METAINT){
                      send_metadata(source->clients[j]->sock_d, &stub);

                      tow = 0;
                      total = 0;
                    }
                    else {
                      tow = METAINT - sent_bytes;
                    }
                  }
                }

              }
            }
          }
          else {

            client_meta *client = spellcast_get_client(srv, i);
            if (client){
              if (FD_ISSET(i, &srv->empty_clients)){

                // will block if client sends metadata without header_end at all
                if (spellcast_client_parse_header(srv, client)){
                  spellcast_print_client_info(client);

                   // register with source
                   spellcast_register_client(srv, client); 

                   // send info about server
                   char *mes = "ICY 200 OK\r\nContent-Type:audio/mpeg\r\nicy-br:96\r\nicy-metaint:8192\r\n\r\n";
                   send_message(client->sock_d, mes, strlen(mes));

                  FD_CLR(i, &srv->empty_clients);
                }
              }
              else {
                printf("client something else\n");

                  int x = recv(client->sock_d, client->buffer, BUFFLEN, 0);
                  if (x == 0){
                    printf(" *** CLIENT on socket %d disconnected ***\n", i);
                    spellcast_disconnect_client(srv, client);
                  }
              }
            }
          }
        }
      } // FD_ISSET main
    }
  } // while(1)

  return 0;
}

void 
spellcast_server_dispose(spellcast_server *srv)
{
  freeaddrinfo(srv->srv_src_addrinfo);
  freeaddrinfo(srv->srv_cl_addrinfo);

  spellcast_dispose_stream_meta(srv->server_metadata->server_data);

  free(srv->server_metadata->notice);
  free(srv->client_port);
  free(srv->source_port);
  free(srv->server_metadata);
  
  spellcast_dispose_icy_protocol_info(srv->icy_p);
  free(srv);
}

void 
spellcast_dispose_stream_meta(stream_meta *stream)
{
  free(stream->name);
  free(stream->genre);
  free(stream->url);
  free(stream->mime_type);
  free(stream);
}

void* 
get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET){
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void 
send_metadata(int socket, icy_metadata *meta)
{
  // 28 is the number of additional characters in metadata without url and title
  int msg_len = ((meta->title != NULL) ? strlen(meta->title) : 0) + ((meta->url != NULL) ? strlen(meta->url) : 0) + 28;
  int padding = 16 - msg_len % 16;
  int to_write = msg_len + padding + 1; 
  char *buf = malloc(to_write);

  memset(buf, 0, to_write);
  sprintf(buf, ICY_METADATA_FORMAT, (msg_len + padding) / 16, meta->title != NULL ? meta->title : "", meta->url != NULL ? meta->url : "");

  send_message(socket, buf, to_write);
  free(buf);
}
