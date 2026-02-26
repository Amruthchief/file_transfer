#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int SOCKET;
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR -1
  #define closesocket close
#endif

#define SIZE 1024

void send_file(FILE *fp, int sockfd){
  int n;
  char data[SIZE] = {0};

  while((n = fread(data, 1, SIZE, fp)) > 0) {
    if (send(sockfd, data, n, 0) == -1) {
      perror("[-]Error in sending file.");
      exit(1);
    }
    memset(data, 0, SIZE);
  }
}

int main(){
#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    printf("[-]WSAStartup failed.\n");
    exit(1);
  }
#endif

  char *ip = "127.0.0.1";
  int port = 8080;
  int e;

  int sockfd;
  struct sockaddr_in server_addr;
  FILE *fp;
  char *filename = "send.txt";

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0) {
    perror("[-]Error in socket");
    exit(1);
  }
  printf("[+]Client socket created successfully.\n");

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ip);

  e = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if(e == -1) {
    perror("[-]Error in connecting to server");
    exit(1);
  }
	printf("[+]Connected to Server.\n");

  fp = fopen(filename, "rb");
  if (fp == NULL) {
    perror("[-]Error in reading file.");
    exit(1);
  }

  send_file(fp, sockfd);
  printf("[+]File data sent successfully.\n");

	printf("[+]Closing the connection.\n");
#ifdef _WIN32
  closesocket(sockfd);
  WSACleanup();
#else
  close(sockfd);
#endif

  return 0;
}
