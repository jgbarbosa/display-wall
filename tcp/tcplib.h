#ifndef TCP_H
#define TCP_H

#include <arpa/inet.h>
#include <atomic>
#include <future>
#include <mutex>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace ospray {
namespace dw {

enum MSGTYPE { BARRIER, RANKED, BROADCAST };

struct msg_header {
  MSGTYPE type;
  int rank;
  size_t size;
};

struct message {
  msg_header header;
  void *buf;
};

struct TCP {

  int sockfd = -1, newsockfd = -1, n, pid;
  struct sockaddr_in serverAddress;
  struct sockaddr_in clientAddress;
  std::future<void> thread;
  std::mutex m;
  std::vector<message> messagequeue;

  volatile int mm = 1;

  TCP();
  ~TCP();

  void startserver(int port);
  void startclient(std::string address, int port);

  bool probe();
  size_t receivemsg(int &rank, size_t &size, MSGTYPE &type, void **buf);
  void sendmsg(const int &rank, const size_t &size, const MSGTYPE &type,
               const void *buf);
  void sendbarrier();

private:
  static void server(void *self);
};
} // namespace dw
} // namespace ospray
#endif
