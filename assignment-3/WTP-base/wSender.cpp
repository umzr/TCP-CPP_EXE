// the main program file for wReceiver

// socket programming
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h> // rand()
#include <ctime>    // to get time as random seed for initial seqNum
#include <chrono>   // timer for resend
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>

// IO
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <fstream> // ifstream
#include <fcntl.h> // open

#include <unordered_map>
#include <vector>
#include <deque>
#include <assert.h>
#include <stdlib.h> /* srand, rand */
#include <time.h>   /* time */

#include "crc32.h"

// define
#define BUFLEN 1500
// #define DEBUG 1

#define CHUNCK_SIZE 1456
#define PACKET_SIZE 1472

#define MAX_PACKET_LEN 1472
#define MAX_BUFFER_LEN 2048
#define MAX_PACKET_SIZE 1472

#define CHUNCK_SIZE 1456
#define PACKET_SIZE 1472
#define RETRANS_TIME 500
#define DEBUG 0

struct PacketHeader
{
    unsigned int type;     // 0: START; 1: END; 2: DATA; 3: ACK
    unsigned int seqNum;   // Described below
    unsigned int length;   // Length of data; 0 for ACK, START and END packets
    unsigned int checksum; // 32-bit CRC
};

void die(const char *s)
{
    perror(s);
    exit(1);
}

void serialize_header_to_unsigned_char(PacketHeader *header, unsigned char *buf)
{
    memcpy(buf, (unsigned char *)&(header->type), 4);
    memcpy(buf + 4, (unsigned char *)&(header->seqNum), 4);
    memcpy(buf + 8, (unsigned char *)&(header->length), 4);
    memcpy(buf + 12, (unsigned char *)&(header->checksum), 4);
}

void deserialize_header(PacketHeader *header, unsigned char *buf)
{
    memcpy((unsigned char *)&(header->type), buf, 4);
    memcpy((unsigned char *)&(header->seqNum), buf + 4, 4);
    memcpy((unsigned char *)&(header->length), buf + 8, 4);
    memcpy((unsigned char *)&(header->checksum), buf + 12, 4);
}

void serialize_packet(PacketHeader *header, unsigned char *buf, unsigned char *data, int length)
{
    serialize_header_to_unsigned_char(header, buf);
    // copy data
    for (int j = 0; j < length; j++)
    {
        buf[16 + j] = data[j];
    }
}

void deserialize_packet(PacketHeader *header, unsigned char *buf, unsigned char *data)
{
    deserialize_header(header, buf);
    if (header->length == 0)
    {
        return;
    }
    buf += 16;
    for (int i = 0; i < header->length; i++)
    {
        *data++ = *buf++;
    }
}

void writeLog(std::ofstream &log, PacketHeader *packet)
{
    log << packet->type << " " << packet->seqNum
        << " " << packet->length << " " << packet->checksum
        << std::endl;
}
void writeCMD(PacketHeader *packet)
{
    if (DEBUG)
        std::cout << packet->type << " " << packet->seqNum
                  << " " << packet->length
                  << " " << packet->checksum << std::endl;
}

void sendWindow(std::deque<unsigned char *> &window, std::deque<std::chrono::time_point<std::chrono::system_clock>> &wTime,
                int sockfd, int start, std::ofstream &logfile, sockaddr_in &si_other)
{
    socklen_t slen = sizeof(si_other);
    int numbytes;
    for (int i = start; i < window.size(); i++)
    {

        unsigned char data[CHUNCK_SIZE];
        memset(data, 0, CHUNCK_SIZE);

        PacketHeader cheader;
        deserialize_packet(&cheader, window[i], data);

        if ((numbytes = sendto(sockfd, window[i], PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen) == -1))
        {
            die("cannot send sendWindow");
        }
        writeLog(logfile, &cheader);
        wTime.push_back(std::chrono::high_resolution_clock::now());
    }
}

void setWindow(std::deque<unsigned char *> &slideWindow, int size, std::istream &input, unsigned int &seqNum)
{

    unsigned char data[CHUNCK_SIZE];
    int length;

    for (int i = 0; i < size; i++)
    {
        input.read((char *)data, CHUNCK_SIZE);
        length = input.gcount();
        if (length <= 0)
        {
            return;
        }

        PacketHeader header;
        header.type = 2; // DATA
        header.seqNum = seqNum++;
        header.length = length;
        header.checksum = crc32(data, length);

        unsigned char *buf = new unsigned char[PACKET_SIZE];
        memset(buf, '\0', PACKET_SIZE);
        serialize_packet(&header, buf, data, length);

        slideWindow.push_back(buf);
    }
}

void sendStartEnd(int sockfd, int type, std::ofstream &logfile, sockaddr_in &si_other)
{
    srand(time(NULL));
    socklen_t slen = sizeof(si_other);

    PacketHeader startEndHeader;
    startEndHeader.type = type;
    startEndHeader.seqNum = (unsigned int)rand();
    startEndHeader.length = 0;
    startEndHeader.checksum = 0;

    unsigned char sbuf[PACKET_SIZE]; // buffer for send data Packets
    memset(sbuf, '\0', PACKET_SIZE);
    serialize_header_to_unsigned_char(&startEndHeader, sbuf);

    int numbytes = sendto(sockfd, sbuf, PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen);
    if (numbytes == -1)
    {
        die("start / end fail to send");
    }

    writeLog(logfile, &startEndHeader);
    writeCMD(&startEndHeader);

    unsigned char rbuf[PACKET_SIZE];
    memset(rbuf, '\0', PACKET_SIZE);
    PacketHeader receiverHeader; // header of response from receiver

    std::chrono::time_point<std::chrono::system_clock> start, current;
    start = std::chrono::high_resolution_clock::now();
    std::chrono::milliseconds msec(RETRANS_TIME);
    std::chrono::duration<double> timeout(msec);

    while (true)
    {
        std::cout << "loop test\n";
        numbytes = recvfrom(sockfd, rbuf, PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr *)&si_other, &slen);
        if (numbytes == -1)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                die("recvfrom");
            }
        }
        else
        {
            deserialize_header(&receiverHeader, rbuf);

            writeLog(logfile, &receiverHeader);
            writeCMD(&receiverHeader);

            if (receiverHeader.type == 3 && receiverHeader.seqNum == startEndHeader.seqNum)
            {
                break;
            }
        }

        // when timeout
        current = std::chrono::high_resolution_clock::now();
        if (current - start >= timeout)
        {
            if ((numbytes = sendto(sockfd, sbuf, PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen)) == -1)
            {
                die("timeout send");
            }
            writeLog(logfile, &startEndHeader);
            start = std::chrono::high_resolution_clock::now();
        }
        else
        {
            std::cout << "not yet the time \n";
        }
    }
}

int main(int argc, char *argv[])
{
    std::string receiver_ip = argv[1], receiver_port = argv[2],
                window_size = argv[3], input_file = argv[4], log = argv[5];
    printf("this is sender\n");
    for (int i = 0; i < argc - 1; i++)
    {
        printf("%s", argv[i]);
        printf(" ");
    }
    unsigned int lowSeqNum = 0;
    // next seqNum to be added
    unsigned int seqNum = 0;
    std::deque<std::chrono::time_point<std::chrono::system_clock>> wTime;
    // create the sldiing window
    std::deque<unsigned char *> slideWindow;

    // open the log file
    if (DEBUG)
        std::cout << "logfs: " << log << std::endl;
    std::ofstream logfs;
    logfs.open(log);
    if (!logfs)
    {
        die("cannot create log");
    }

    // open the target file in binary mode
    if (DEBUG)
        std::cout << "input_file: " << input_file << std::endl;
    std::ifstream input(input_file, std::ios::binary);
    if (!input)
    {
        die("cannot open file");
    }

    struct sockaddr_in si_other;
    int s;
    socklen_t slen = sizeof(si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }

    memset((unsigned char *)&si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(stoi(receiver_port));
    struct hostent *sp = gethostbyname(receiver_ip.c_str());
    memcpy(&si_other.sin_addr, sp->h_addr, sp->h_length);

    // send start
    sendStartEnd(s, 0, logfs, si_other); //  start
    if (DEBUG)
        printf("sendConnection done \n");

    setWindow(slideWindow, stoi(window_size), input, seqNum);
    if (DEBUG)
        printf("setWindow done \n");

    sendWindow(slideWindow, wTime, s, 0, logfs, si_other);
    if (DEBUG)
        printf("sendWindow done \n");

    unsigned char buffer[PACKET_SIZE];
    memset(buffer, '\0', PACKET_SIZE);
    int ack_pack, old_size, add_size, start;
    std::chrono::time_point<std::chrono::system_clock> current_time;
    std::chrono::milliseconds msec(RETRANS_TIME);
    std::chrono::duration<double> timeout(msec);

    int numbytes;
    while (true)
    {
        memset(buffer, '\0', PACKET_SIZE);
        numbytes = recvfrom(s, buffer, PACKET_SIZE, 0, (struct sockaddr *)&si_other, &slen);
        if (numbytes == -1)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            { // if not timeout
                die("recvfrom");
            }
        }
        else
        {
            PacketHeader header;
            deserialize_header(&header, buffer);
            writeLog(logfs, &header);
            // logfs << header.type << '\t' << header.seqNum << '\t' << header.length << '\t' << header.checksum << std::endl;
            // std::cout << header.type << '\t' << header.seqNum << '\t' << header.length << '\t' << header.checksum << std::endl;
            if (header.type == 3)
            { // ACK
                if (header.seqNum > lowSeqNum)
                {
                    ack_pack = header.seqNum - lowSeqNum; // # of acked packets, max # of new packets added to the window
                    // delete from window
                    for (int i = 0; i < ack_pack; i++)
                    {
                        delete[] slideWindow[0];
                        slideWindow.pop_front();
                        wTime.pop_front();
                    }
                    old_size = slideWindow.size();
                    setWindow(slideWindow, ack_pack, input, seqNum);
                    add_size = slideWindow.size() - old_size;
                    start = slideWindow.size() - add_size;
                    sendWindow(slideWindow, wTime, s, start, logfs, si_other);
                    lowSeqNum = header.seqNum;
                }
            }
        }

        if (slideWindow.size() == 0)
        {
            break;
        }
        assert(!wTime.empty());
        current_time = std::chrono::high_resolution_clock::now();
        if (current_time - wTime[0] >= timeout)
        {
            wTime.clear();                                         // empty times before reset
            sendWindow(slideWindow, wTime, s, 0, logfs, si_other); // send the whole window
        }
    }

    sendStartEnd(s, 1, logfs, si_other);

    return 0;
}
