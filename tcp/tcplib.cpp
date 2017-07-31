#include "tcplib.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
namespace ospray {
namespace dw {
void TCP::server(void *self_) {
  TCP *self = (TCP *)self_;
  int n;
  int newsockfd = self->newsockfd;
  int size;
  pthread_detach(pthread_self());

  do {
    size = 0;
    n = recv(newsockfd, &size, sizeof(size_t), 0);
    if (n == sizeof(size_t)) {
      void *ptr = std::malloc(size);
      n = recv(newsockfd, ptr, size, 0);
      if (n == size) {
        message msg;
        msg.header.rank = *(int *)ptr;
        msg.header.type = *(MSGTYPE *)((char *)ptr + sizeof(int));
        msg.header.size = n - (sizeof(int) + sizeof(MSGTYPE));
        if (msg.header.type != BARRIER) {
          msg.buf = std::malloc(msg.header.size);
          std::memcpy(msg.buf, (char *)ptr + (sizeof(int) + sizeof(MSGTYPE)),
                      msg.header.size);
        }
        std::lock_guard<std::mutex> l(self->m);
        self->messagequeue.push_back(msg);
        std::free(ptr);
      }
    }
  } while (self->mm == 1);

  std::cout << "Exit ... " << std::endl;
}

TCP::TCP() {}

TCP::~TCP() {
  // running = false;
  mm = 0;
  thread.wait();

  if (sockfd != -1)
    close(sockfd);
  close(newsockfd);
}

void TCP::startserver(int port) {
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&serverAddress, 0, sizeof(serverAddress));
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddress.sin_port = htons(port);
  bind(sockfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
  listen(sockfd, 5);
  std::string str;
  socklen_t sosize = sizeof(clientAddress);
  newsockfd = accept(sockfd, (struct sockaddr *)&clientAddress, &sosize);
  str = inet_ntoa(clientAddress.sin_addr);
  // pthread_create(&serverThread, NULL, &server, (void *)this);
  thread = std::async(std::launch::async, server, this);
}

void TCP::startclient(std::string address, int port) {

  newsockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (newsockfd == -1) {
    std::cout << "Could not create socket" << std::endl;
  }

  if (inet_addr(address.c_str()) == -1) {
    struct hostent *he;
    struct in_addr **addr_list;
    if ((he = gethostbyname(address.c_str())) == NULL) {
      herror("gethostbyname");
      std::cout << "Failed to resolve hostname\n";
      return;
    }
    addr_list = (struct in_addr **)he->h_addr_list;
    for (int i = 0; addr_list[i] != NULL; i++) {
      serverAddress.sin_addr = *addr_list[i];
      break;
    }
  } else {
    serverAddress.sin_addr.s_addr = inet_addr(address.c_str());
  }
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port);
  if (connect(newsockfd, (struct sockaddr *)&serverAddress,
              sizeof(serverAddress)) < 0) {
    perror("connect failed. Error");
    return;
  }
  // if (running)
  //   return;
  // running = true;
  // pthread_create(&serverThread, NULL, &server, (void *)this);
  thread = std::async(std::launch::async, server, this);
}
bool TCP::probe() {
  std::lock_guard<std::mutex> l(m);
  return messagequeue.size() > 0;
}

size_t TCP::receivemsg(int &rank, size_t &size, MSGTYPE &type, void **buf) {

  if (!messagequeue.empty()) {
    std::lock_guard<std::mutex> lock(m);
    message msg = messagequeue[0];

    rank = msg.header.rank;
    size = msg.header.size;
    type = msg.header.type;
    *buf = msg.buf;

    messagequeue.erase(messagequeue.begin());
    return size;
  }
  return 0;
}

void TCP::sendmsg(const int &rank, const size_t &size, const MSGTYPE &type,
                  const void *buf) {

  size_t buf_size = sizeof(int) + sizeof(MSGTYPE) + size;
  char *msg = (char *)std::malloc(buf_size);
  int *mrank = (int *)msg;
  *mrank = rank;
  MSGTYPE *mtype = (MSGTYPE *)(&msg[sizeof(int)]);
  *mtype = type;
  std::memcpy(&msg[sizeof(int) + sizeof(MSGTYPE)], buf, size);
  write(newsockfd, &buf_size, sizeof(size_t));
  write(newsockfd, msg, buf_size);
}

void TCP::sendbarrier() {
  size_t buf_size = sizeof(int) + sizeof(MSGTYPE);
  char *msg = (char *)std::malloc(buf_size);
  int *mrank = (int *)msg;
  *mrank = -1;
  MSGTYPE *mtype = (MSGTYPE *)(&msg[sizeof(int)]);
  *mtype = BARRIER;
  write(newsockfd, &buf_size, sizeof(size_t));
  write(newsockfd, msg, buf_size);
}
} // namespace dw
} // namespace ospray
// void TCP::clean() {}
