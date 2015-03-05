#ifndef WIN32
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#endif
#include <iostream>
#include <udt.h>
#include "test_util.h"

using namespace std;

#define WRAPPER extern "C"

void* recvdata(void*);

class ServerThread
{
public:
    UDTSOCKET recver;
    char* data;
    int udt_buff;
    ServerThread(UDTSOCKET *socket);
    int read(char* buff, int len);
    int read_into(int fd, int len);
    int close();
    int send_stuff(char *data, int size);
};

class Server
{
public:
    int blast;
    int blast_rate;
    int udt_buff;
    int udp_buff;
    int mss;
    UDTSOCKET serv;

    Server();
    int close();
    ServerThread *next_client();
    int start(char *host, char *port);
    int set_buffer_size(int size);

};


class Client
{
public:
    int blast;
    int blast_rate;
    int udt_buff;
    int udp_buff;
    int mss;
    UDTSOCKET client;

    Client();
    int close();
    int start(char *host, char *port);
    int send_stuff(char *data, int size);
    int read(char* buff, int len);
};


Server::Server()
{
    blast = 0;
    blast_rate = 0;
    udt_buff = 67108864;
    udp_buff = 67108864;
    mss = 8000;
}

int Server::start(char *host, char *port)
{
    // setup socket
    addrinfo hints;
    addrinfo* res;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Setup address information
    if (0 != getaddrinfo(NULL, port, &hints, &res)){
        cout << "illegal port number or port is busy: [" << port << "]" << endl;
        return 0;
    }
    // Create the server socket
    serv = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    // Bind the server socket
    if (UDT::ERROR == UDT::bind(serv, res->ai_addr, res->ai_addrlen)){
        cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }
    freeaddrinfo(res);

    // Listen on the port
    if (UDT::ERROR == UDT::listen(serv, 10)){
        cout << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }
    cout << "server is ready at port: " << port << endl;

    // Incoming connections should be recieved by calling
    // next_client() repeatedly
    return 0;
}

int Server::close()
{
    UDT::close(serv);
    return 0;
}

ServerThread *Server::next_client(){

    UDTSOCKET recver;
    sockaddr_storage clientaddr;
    int addrlen = sizeof(clientaddr);

    // Wait for the next connection
    recver = UDT::accept(serv, (sockaddr*)&clientaddr, &addrlen);
    if (UDT::INVALID_SOCK == recver){
        cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }

    // Get the client information
    char clienthost[NI_MAXHOST];
    char clientservice[NI_MAXSERV];
    getnameinfo((sockaddr *)&clientaddr,
                addrlen,
                clienthost,
                sizeof(clienthost),
                clientservice,
                sizeof(clientservice),
                NI_NUMERICHOST|NI_NUMERICSERV);

    cout << "new connection: " << clienthost << ":" << clientservice << endl;

    // Return a ServerThread object that belongs to the new client
    return new ServerThread(new UDTSOCKET(recver));
}

Client::Client()
{
    blast = 0;
    blast_rate = 0;
    udt_buff = 67108864;
    udp_buff = 67108864;
    mss = 8000;
}

int Client::start(char *host, char *port)
{
    struct addrinfo hints, *local, *peer;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (0 != getaddrinfo(NULL, port, &hints, &local)){
        cout << "incorrect network address.\n" << endl;
        return 0;
    }

    client = UDT::socket(local->ai_family, local->ai_socktype,
                         local->ai_protocol);
    freeaddrinfo(local);

    if (0 != getaddrinfo(host, port, &hints, &peer)){
        cout << "incorrect server/peer address. " << host << ":" << port
             << endl;
        return 0;
    }

    // connect to the server, implicit bind
    if (UDT::ERROR == UDT::connect(client, peer->ai_addr, peer->ai_addrlen)){
        cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }
    freeaddrinfo(peer);
    return 0;
}

int Client::close()
{
    UDT::close(client);
    return 0;
}

ServerThread::ServerThread(UDTSOCKET *usocket){
    recver = *(UDTSOCKET*)usocket;
    delete (UDTSOCKET*)usocket;
}

int ServerThread::close(){
    return UDT::close(recver);
}

int send_data(UDTSOCKET socket, char *data, int size)
{
    int ss = 0;
    int ssize = 0;
    cout << "sending " << size << " bytes." << endl;
    while (ssize < size) {
        // Send as much data as we can
        ss = UDT::send(socket, data + ssize, size - ssize, 0);
        // Check for errors
        if (UDT::ERROR == ss){
            cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
        }
        // Increment the amount sent before repeating
        ssize += ss;
    }
    cout << "sent " << ssize << " bytes." << endl;
    return ssize;
}

int read_data(UDTSOCKET socket, char *buff, int len)
{
    cout << "reading from socket: " << socket << endl;
    int rs = UDT::recv(socket, buff, len, 0);
    if (UDT::ERROR == rs) {
        if (UDT::getlasterror().getErrorCode() != 2001)
            cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
        return -1;
    }
    return rs;
}

int Client::send_stuff(char *data, int size){
    return send_data(client, data, size);
}

int ServerThread::send_stuff(char *data, int size){
    return send_data(recver, data, size);
}

int ServerThread::read(char* buff, int len){
    return read_data(recver, buff, len);
}

int Client::read(char* buff, int len){
    return read_data(client, buff, len);
}

/***********************************************************************
 *            Wrappers for CTypes Python bindings
 ***********************************************************************/

WRAPPER Client* new_client(){
    return new Client();
}

WRAPPER int client_start(Client *client, char *host, char *port){
    return client->start(host, port);
}

WRAPPER int client_send(Client *client, char *data, int len){
    return client->send_stuff(data, len);
}

WRAPPER int client_close(Client *client){
    client->close();
    delete client;
    return 0;
}

WRAPPER int client_read(Client *client, char *buff, int len){
    return client->read(buff, len);
}

WRAPPER Server* new_server(){
    return new Server();
}

WRAPPER ServerThread* server_next_client(Server *server){
    return server->next_client();
}

WRAPPER int sthread_read(ServerThread *sthread, char *buff, int len){
    return sthread->read(buff, len);
}

WRAPPER int sthread_send(ServerThread *sthread, char *data, int len){
    return sthread->send_stuff(data, len);
}

WRAPPER int server_set_buffer_size(Server *server, int size){
    server->udt_buff = size;
    return 0;
}

WRAPPER int server_start(Server *server, char *host, char *port){
    return server->start(host, port);
}

WRAPPER int server_close(Server *server){
    cout << "closing server" << endl;
    server->close();
    delete server;
    return 0;
}

WRAPPER int sthread_close(Server *sthread){
    cout << "closing server thread" << endl;
    sthread->close();
    delete sthread;
    return 0;
}