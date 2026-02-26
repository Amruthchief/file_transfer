#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
#else
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int SOCKET;
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR -1
  #define closesocket close
#endif

#define SIZE 1024

void write_file(int sockfd){
  int n;
  FILE *fp;
  char *filename = "recv.txt";
  char buffer[SIZE];

  fp = fopen(filename, "wb");
  if (fp == NULL) {
    perror("[-]Error in creating file");
    return;
  }

  while (1) {
    n = recv(sockfd, buffer, SIZE, 0);
    if (n <= 0){
      break;
    }
    fwrite(buffer, 1, n, fp);
    memset(buffer, 0, SIZE);
  }
  fclose(fp);
  return;
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

  int sockfd, new_sock;
  struct sockaddr_in server_addr, new_addr;
  socklen_t addr_size;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0) {
    perror("[-]Error in socket");
    exit(1);
  }
  printf("[+]Server socket created successfully.\n");

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ip);

  e = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if(e < 0) {
    perror("[-]Error in bind");
    exit(1);
  }
  printf("[+]Binding successfull.\n");

  if(listen(sockfd, 10) == 0){
		printf("[+]Listening....\n");
	}else{
		perror("[-]Error in listening");
    exit(1);
	}

  addr_size = sizeof(new_addr);
  new_sock = accept(sockfd, (struct sockaddr*)&new_addr, &addr_size);
  if (new_sock < 0) {
    perror("[-]Error in accept");
    exit(1);
  }
  
  write_file(new_sock);
  printf("[+]Data written in the file successfully.\n");

#ifdef _WIN32
  closesocket(new_sock);
  closesocket(sockfd);
  WSACleanup();
#else
  close(new_sock);
  close(sockfd);
#endif

  return 0;
}
