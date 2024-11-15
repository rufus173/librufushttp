//================ headers =============
//#include "tcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
//================ macros ==============
#define CHECK_SEND(result,return_val) \
if (result < 0){\
	perror("send");\
	return return_val;\
}
//=================== structs ================
struct http_header {
	char *field_name;
	char *field_value;
	void *next;
};
struct http_connection {
	int socket;
	char *version_string;
	struct addrinfo *address_info;
};
struct http_request {
        char *method;
        char *url;
	char *body;
	struct http_header *header;
};
struct http_response {
	char *response_line;
	
};
//================== prototypes ===============
int http_request_append_header(struct http_request *request, char *field_name, char *field_value);
//================== public funcitons =============
struct http_connection *http_connect(char *host){
	//setup returned struct
	struct http_connection *connection;
	connection = malloc(sizeof(struct http_connection));
	memset(connection,0,sizeof(struct http_connection));
	if (connection == NULL){
		perror("malloc");
		return NULL;
	}

	//resolve a hostname or url
	struct addrinfo hints, *address_info;
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int result = getaddrinfo(host,"80",&hints,&address_info);
	if (result < 0){
		fprintf(stderr,"getaddrinfo: %s\n",gai_strerror(result));
		return NULL;
	}
	connection->address_info = address_info;

	//create a socket
	int sock = socket(AF_INET,SOCK_STREAM,0);
	if (sock < 0){
		perror("socket");
		return NULL;
	}
	connection->socket = sock;

	//connect
	result = connect(sock,address_info->ai_addr,address_info->ai_addrlen);
	if (result < 0){
		perror("connect");
		return NULL;
	}

	return connection;
}
int http_disconnect(struct http_connection *connection){
	int status = close(connection->socket);
	if (status < 0){
		perror("close");
	}
	freeaddrinfo(connection->address_info);
	free(connection);
	return 0;
}
int http_send_request(struct http_connection *connection,struct http_request *request){
	//add content length header
	http_request_append_header(request,"content-length","0");

	//setup a buffer for the full request
	char *full_request = NULL;
	long long int full_request_size = 0;

	//add the request line
	int request_line_size = snprintf(NULL,0,"%s %s HTTP/1.1\r\n",request->method,request->url)+1;//get size of request line
	full_request_size += request_line_size;
	full_request = realloc(full_request,full_request_size);
	snprintf(full_request,request_line_size,"%s %s HTTP/1.1\r\n",request->method,request->url);
	
	//add the headers
	struct http_header *node;
	node = request->header;
	for (int node_count = 0;;node_count++){
		if (node == NULL){
			break;
		}
		int header_length = snprintf(NULL,0,"%s: %s\r\n",node->field_name,node->field_value)+1;
		full_request_size += header_length;
		full_request = realloc(full_request,full_request_size);
		char *header_buffer = malloc(header_length);
		snprintf(header_buffer,header_length,"%s: %s\r\n",node->field_name,node->field_value);
		strcat(full_request,header_buffer);
		//next node
		free(header_buffer);
		struct http_header *old_node;
		old_node = node;
		node = old_node->next;
	}

	//mark the start of the body
	// body seperated by \r\n\r\n
	full_request_size += 2;
	full_request = realloc(full_request,full_request_size);
	strcat(full_request,"\r\n");

	printf("%s",full_request);
	//send the full request packet
	char *send_buffer = full_request;
	long long int bytes_to_send = full_request_size;
	for (;;){
		int bytes_sent = send(connection->socket,send_buffer,bytes_to_send,0);
		if (bytes_sent < 0){
			perror("send");
			return -1;
		}
		bytes_to_send =- bytes_sent;
		send_buffer += bytes_sent;
		if (bytes_to_send <= 0){
			break;
		}
	}
	free(full_request);

	return 0;
}
struct http_request *http_generate_request(char *method, char *url){
	struct http_request *request;
	request = malloc(sizeof(struct http_request));
	if (request == NULL){
		return NULL;
	}
	request->method = malloc(strlen(method)+1);
	request->url = malloc(strlen(url)+1);
	if (request->method == NULL || request->url == NULL){
		perror("malloc");
		return NULL;
	}
	snprintf(request->method,strlen(method)+1,"%s",method);
	snprintf(request->url,strlen(url)+1,"%s",url);
	request->body = NULL;
	request->header = NULL;
	return request;
}
int http_free_request(struct http_request *request){
	free(request->method);
	free(request->url);
	//free the linked list of headers
	struct http_header *node;
	node = request->header;
	for (;;){
		if (node == NULL){
			break;
		}
		struct http_header *prev_node = node;
		node = prev_node->next;
		//printf("freeing header \"%s: %s\"\n",prev_node->field_name,prev_node->field_value);
		free(prev_node->field_name);
		free(prev_node->field_value);
		free(prev_node);
	}
	free(request);
	return 0;
}
struct http_response *http_receive_response(struct http_connection *connection){
	char buffer[4096];
	int buffer_size = sizeof(buffer);
	int bytes_received = recv(connection->socket,buffer,buffer_size,0);
	if (bytes_received < 0){
		perror("recv");
		return NULL;
	}
	printf("got %d bytes\n",bytes_received);
	printf("%s",buffer);
	return NULL;
}
int http_request_append_header(struct http_request *request, char *field_name, char *field_value){
	//find the tail node
	struct http_header *next_node;
	next_node = request->header;
	struct http_header **new_node_location = &request->header;
	for (;;){
		if (next_node == NULL){
			break;
		}
		struct http_header *node;
		node = next_node;
		next_node = node->next;
		new_node_location = (struct http_header **)&node->next;
	}
	//append new struct
	struct http_header *new_node = malloc(sizeof(struct http_header));
	if (new_node == NULL){
		perror("malloc");
		return -1;
	}
	memset(new_node, 0, sizeof(struct http_header));
	new_node->next = NULL;
	new_node->field_name = malloc(strlen(field_name)+1);
	new_node->field_value = malloc(strlen(field_value)+1);
	snprintf(new_node->field_name,strlen(field_name)+1,"%s",field_name);
	snprintf(new_node->field_value,strlen(field_value)+1,"%s",field_value);
	*new_node_location = new_node;
	return 0;
}
