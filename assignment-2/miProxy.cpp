#include <stdio.h>
#include <string.h>
#include <fstream>
#include <netdb.h>
#include <time.h>
#include <cstddef>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <iostream>  // std::cout
#include <algorithm> // std::sort
#include <vector>    // std::vector

#define MESSAGE_SIZE 1024
#define DEBUG 1
#define TURE 1
#define FALSE 0
#define SER_PORT 80
#define MAXCLIENTS 30
#define HEADERBUF_SZ 1000

using namespace std;

int get_server_socket(struct sockaddr_in *address, int PORT)
{
    int yes = 1;
    int server_socket;
    // create a master socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket <= 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int success =
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (success < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    // type of socket created
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(PORT);

    // bind the socket to localhost port 8listening88
    success = bind(server_socket, (struct sockaddr *)address, sizeof(*address));
    if (success < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("---Listening on port %d---\n", PORT);

    // try to specify maximum of 3 pending connections for the server socket
    if (listen(server_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return server_socket;
}

int N_server_socket(int PORT, char *server_ip)
{
    int serverfd;
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(SER_PORT);
    struct hostent *host = gethostbyname(server_ip);
    if (host == nullptr)
    {
        fprintf(stderr, "%s: unknown host\n", server_ip);
        return -1;
    }

    memcpy(&server.sin_addr, host->h_addr, host->h_length);
    // create server socket to communicate with server
    serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // if there are errors, socket return -1
    if (serverfd < 0)
    {
        perror("ERROR opening socket");
        return -1;
    }

    if (connect(serverfd, (sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("ERROR connecting stream\n");
        return -1;
    }

    printf("connect to web server\n");
    return serverfd;
}

// void log1(string log_path, string  ip, string chunkName, string sin_addr , std::chrono::duration<double> d , float currentThroughput, float throughput, float chosenBitrate ){
//     std::ofstream f;
//     f.open(log_path, std::ofstream::out | std::ofstream::app);
//     if (f.is_open()){
//     //<browser-ip> <chunkname> <server-ip> <duration> <tput> <avg-tput> <bitrate>
//     f << ip
//             << " "+ chunkName + " "
//                 << sin_addr
//                     << std::fixed << d.count() << " "
//                          << std::fixed << currentThroughput << " "
//                              << std::fixed << chosenBitrate << "\n" ;
//     f.close();
//     } else {
//       cout << "Error while opening the file";
//     }
// }

string getbuffer(int serverfd)
{
    char Buffer[1000];

    memset(Buffer, 0, HEADERBUF_SZ);
    int noInput;
    int i = 0;
    char endOfHeader[] = "\r\n\r\n";
    char *ptr = NULL;
    int byteReceived;
    char msg;
    while (ptr == NULL)
    {
        byteReceived = read(serverfd, &msg, 1);
        if (byteReceived < 0)
        {
            perror("receive");
        }
        else if (byteReceived == 0)
        {
            noInput = 1;
            break;
        }
        Buffer[i] = msg;
        i++;
        ptr = strstr(Buffer, endOfHeader);
    }

    return string(Buffer);
}

int nodns(int listen_port, char *server_ip, float alpha, char *log_path)
{
    // ref t04
    double tputs[30];
    string s, length;
    vector<int> bitrates;
    char message, *ptr = NULL, *bufPtr = NULL, headerBuf[HEADERBUF_SZ], *ips[MAXCLIENTS];
    int master_socket, addrlen, new_socket, client_socket[MAXCLIENTS], server_socket[MAXCLIENTS],
        client_sock, serverfd, activity, i, j, k, valread, max_sd;

    struct sockaddr_in address;

    fd_set readfds;

    // reset
    for (i = 0; i < MAXCLIENTS; i++)
    {
        client_socket[i] = 0;
        server_socket[i] = 0;
        tputs[i] = 0;
        ips[i] = NULL;
    }

    master_socket = get_server_socket(&address, listen_port);

    // accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");

    while (1)
    {
        // clear the socket set
        FD_ZERO(&readfds);
        // add master socket to set

        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // add child sockets to set
        for (i = 0; i < MAXCLIENTS; i++)
        {
            // socket descriptor
            client_sock = client_socket[i];

            // if valid socket descriptor then add it to read list
            if (client_sock > 0)
                FD_SET(client_sock, &readfds);

            // highest file descriptor number, need it for the select function
            if (client_sock > max_sd)
                max_sd = client_sock;
            // cout << "max_sd: " << max_sd << endl;
        }
        activity = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR))
        {
            perror("select error");
        }
        // If something happened on the master socket ,
        // then its an incoming connection, call accept()
        if (FD_ISSET(master_socket, &readfds))
        {
            int new_socket = accept(master_socket, (struct sockaddr *)&address,
                                    (socklen_t *)&addrlen);
            if (new_socket < 0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            printf("\n---New host connection---\n");
            printf("socket fd is %d , ip is : %s , port : %d \n", new_socket,
                   inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            //******************

            serverfd = N_server_socket(listen_port, server_ip);

            // reseive
            for (int i = 0; i < MAXCLIENTS; i++)
            {
                if (client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    server_socket[i] = serverfd;
                    ips[i] = inet_ntoa(address.sin_addr);
                    break;
                }
            }
        }

        double tput, curTput = 0;
        string xml, chunk, bitstr;
        unsigned first, last, InWord;
        int rate, ifTime = 0, byteSent = 0, rateStart, rateEnd, chosenBitrate = 10,
                  ischunk = 0, bytesRecvd = 0, buf_size = 0;
        char endWord[] = "\r\n\r\n";
        std::chrono::steady_clock::time_point before;
        std::chrono::steady_clock::time_point after;
        std::chrono::duration<double> duration;

        // else some IO operation on some other socket
        for (k = 0; k < MAXCLIENTS; k++)
        {
            client_sock = client_socket[k];
            serverfd = server_socket[k];
            tput = tputs[k];

            // Note: sd == 0 is our default here by fd 0 is actually stdin
            if (client_sock > 0 && FD_ISSET(client_sock, &readfds))
            {
                // Check if it was for closing , and also read the
                // incoming message
                // receive message from client

                memset(headerBuf, 0, sizeof(headerBuf));
                i = 0;
                ischunk = 0;
                ptr = NULL;
                // reverce the data
                while (ptr == NULL)
                {
                    bytesRecvd = read(client_sock, &message, 1);
                    // cout << "byteReceived: "  <<byteReceived << endl;
                    if (bytesRecvd < 0)
                    {
                        perror("receive");
                    }
                    else if (bytesRecvd == 0)
                    {
                        ischunk = 1;
                        break;
                    }
                    headerBuf[i] = message;
                    i++;
                    ptr = strstr(headerBuf, endWord);
                    // cout << "ptr: "  << ptr << endl;
                }
                // close server client
                if (ischunk == 1)
                {
                    close(serverfd);
                    close(client_sock);
                    client_socket[k] = 0;
                    server_socket[k] = 0;
                    tputs[k] = 0;
                    cout << "the browser is off" << endl;
                    exit(1);
                }

                // parse out the get content and see if it is big_buck_bunny.f4m
                if (DEBUG)
                    cout << "headerBuffer \n"
                         << headerBuf << endl;
                string s = string(headerBuf);
                InWord = s.find("/vod/big_buck_bunny.f4m");

                // get xml. find bitrates amd modity initial header
                if (InWord != -1 && (bitrates.size() == 0))
                {
                    // Modify the HTTP GET request as “_nolist.f4m” file request
                    s.replace(s.find(".f4m"), 0, "_nolist");
                    char tmpHeader[s.size() + 1];
                    strcpy(tmpHeader, s.c_str());

                    // Send BOTH the original f4m file request and the modified “_nolist.f4m” request to server
                    byteSent = send(serverfd, headerBuf, i, 0);
                    // Keep the received original f4m response (for parsing bitrates) and not forward it to browser
                    memset(headerBuf, 0, HEADERBUF_SZ);
                    j = 0;
                    ischunk = 0;
                    ptr = NULL;

                    while (ptr == NULL)
                    {
                        bytesRecvd = read(serverfd, &message, 1);
                        if (bytesRecvd < 0)
                        {
                            perror("receive");
                        }
                        else if (bytesRecvd == 0)
                        {
                            ischunk = 1;
                            break;
                        }
                        headerBuf[j] = message;
                        j++;
                        ptr = strstr(headerBuf, endWord);
                    }
                    //*****************

                    // Received video chunk request:
                    s = string(headerBuf);
                    cout << "headerBuf : \n"
                         << s << endl;
                    first = s.find("Content-Length: ");
                    last = s.find("Keep-Alive");
                    length = s.substr(first + 16, last - first);
                    int contentLength = atoi(length.c_str());

                    // dynamic buffer
                    if (buf_size < contentLength)
                    {
                        char cntbuf[contentLength * 2];
                        bufPtr = cntbuf;
                        buf_size = contentLength * 2;
                    }
                    // Forward the modified request to server

                    bytesRecvd = recv(serverfd, bufPtr, contentLength, MSG_WAITALL);

                    xml = string(bufPtr);
                    rateStart = -1;
                    // gothrough all bitrates
                    while ((rateStart = xml.find("bitrate=\"", rateStart + 1)) != -1)
                    {
                        rateEnd = xml.find("\"", rateStart + 9);
                        bitstr = xml.substr(rateStart + 9, rateEnd - rateStart - 9);
                        rate = atoi(bitstr.c_str());
                        bitrates.push_back(rate);
                    }

                    sort(bitrates.begin(), bitrates.end());

                    memset(headerBuf, 0, sizeof(headerBuf));
                    strcpy(headerBuf, tmpHeader);

                    i = i + 7;
                }

                // Forward the modified request to server bitrate modification
                s = string(headerBuf);
                InWord = s.find("Seg");
                ifTime = 0;

                if (InWord != -1)
                {
                    ifTime = 1;
                    // if throughput is not calculated yet, use the minimum bitrate
                    if (tput == 0)
                    {
                        chosenBitrate = bitrates[0];
                    }
                    else
                    {
                        for (int tmp_r : bitrates)
                        {
                            if (tput >= (double)tmp_r * 1.5)
                                chosenBitrate = tmp_r;
                            else
                                break;
                        }
                    }
                    // modify the header;
                    rateStart = s.find("vod/");
                    s.replace(rateStart + 4, InWord - rateStart - 4, to_string(chosenBitrate));
                    i = s.length();
                    memset(headerBuf, 0, sizeof(headerBuf));
                    strcpy(headerBuf, s.c_str());
                }
                before = chrono::steady_clock::now();
                // count
                // find chunk
                s = string(headerBuf);
                first = s.find("GET ");
                last = s.find(" HTTP/1");
                chunk = s.substr(first + 4, last - first - 4);

                byteSent = send(serverfd, headerBuf, i, 0);

                //***
                memset(headerBuf, 0, HEADERBUF_SZ);
                i = 0;
                ischunk = 0;
                ptr = NULL;
                while (ptr == NULL)
                {
                    bytesRecvd = read(serverfd, &message, 1);
                    if (bytesRecvd < 0)
                    {
                        perror("receive");
                    }
                    else if (bytesRecvd == 0)
                    {
                        ischunk = 1;
                        break;
                    }
                    headerBuf[i] = message;
                    i++;
                    ptr = strstr(headerBuf, endWord);
                }

                cout << "NOINPUT " << ischunk << endl;
                s = string(headerBuf);

                first = s.find("Content-Length: ");
                last = s.find("Keep-Alive");
                length = s.substr(first + 16, last - first);
                int chunkLen = atoi(length.c_str());
                cout << "length = " << chunkLen << endl;

                // dynamic buffer
                if (buf_size < chunkLen)
                {
                    char conBuffer[chunkLen * 2];
                    bufPtr = conBuffer;
                    buf_size = chunkLen * 2;
                }

                bytesRecvd = recv(serverfd, bufPtr, chunkLen, MSG_WAITALL);

                after = chrono::steady_clock::now();
                duration = after - before;
                curTput = ((double)chunkLen * 8 / (duration.count() * 1000));

                if (tput == 0)
                {
                    tput = curTput;
                }
                if (ifTime)
                {
                    tput = alpha * curTput + tput * (1 - alpha);
                    tputs[k] = tput;
                }

                byteSent = send(client_sock, headerBuf, i, 0);

                byteSent = send(client_sock, bufPtr, chunkLen, 0);

                // print log
                //  log1(log_path, string(ips[k]) , string(chunk) , string(inet_ntoa(address.sin_addr)) , duration, curTput , tput , chosenBitrate);
                myfile.open(log_path, std::ofstream::out | std::ofstream::app);
                if (myfile.is_open())
                {
                    //<browser-ip> <chunkname> <server-ip> <duration> <tput> <avg-tput> <bitrate>
                    // myfile << "Now we start writing";
                    // myfile << "1 This is ips[k]\n";
                    myfile << ips[k];
                    // myfile << "2 This is chunkName\n";
                    myfile << " " + chunk + " ";
                    // myfile << &server_ip << " ";
                    myfile << inet_ntoa(address.sin_addr);
                    // myfile << "4 This is d.count\n";
                    myfile << duration.count() << " ";
                    // myfile << "5 This is currentThroughput\n";
                    myfile << curTput << " ";
                    // myfile << "6 This is throughput\n";
                    myfile << tput << " ";
                    // myfile << "7 This is chosenBitrate\n";
                    myfile << std::fixed << std::setprecision(2) << chosenBitrate << "\n";
                    myfile.close();
                }
                else
                    cout << "Error while opening the file";
            }
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    std::ofstream f;
    if (argc == 6)
    {
        string mode = string(argv[1]);
        if (mode != "--nodns")
        {
            cout << "Error: missing or extra arguments" << endl;
            return 1;
        }

        int listen_port = atoi(argv[2]);
        char *www_ip = argv[3];
        float alpha = atof(argv[4]);
        if (alpha < 0 || alpha > 1)
        {
            cout << "Error: alpha should be [0,1]" << endl;
            return 1;
        }
        char *log_path = argv[5];
        f.open(log_path, std::ofstream::out | std::ofstream::trunc);
        f.close();

        nodns(listen_port, www_ip, alpha, log_path);
    }
    return 0;
}
