
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
#include <sys/stat.h>
#include "crc32.h"

#define CHUNCK_SIZE 1456
#define PACKET_SIZE 1472
#define DEBUG 0

void die(const char *s)
{
    perror(s);
    exit(1);
}
struct PacketHeader
{
    unsigned int type;     // 0: START; 1: END; 2: DATA; 3: ACK
    unsigned int seqNum;   // Described below
    unsigned int length;   // Length of data; 0 for ACK, START and END packets
    unsigned int checksum; // 32-bit CRC
};

struct DataPacket
{
    unsigned char *data;
    unsigned int length;
};

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

int main(int argc, char *argv[])
{
    for (int i = 0; i < argc - 1; i++)
    {
        printf("%s", argv[i]);
        printf(" ");
    }
    std::cout << std::endl;
    bool end_flag = false;
    std::string port_num = argv[1], window_size = argv[2],
                output_dir = argv[3], log_dir = argv[4];

    std::cout << "this is rserver :" << port_num << " " << window_size << " " << output_dir << " " << log_dir << std::endl;

    std::string ss = "." + output_dir;

    int cheack = rmdir(ss.c_str());
    // return 0;
    int status = mkdir(ss.c_str(), 0777);
    struct sockaddr_in si_me, si_other, sender_ip;
    int receive_num = 0;
    int s, recv_len;
    socklen_t slen = sizeof(si_other);

    // create a UDP socket
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }

    // zero out the structure
    memset((unsigned char *)&si_me, 0, sizeof(si_me));
    memset((unsigned char *)&sender_ip, 0, sizeof(sender_ip));

    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(stoi(port_num));
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind socket to port
    if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
    {
        die("bind");
    }

    int numbytes;
    printf("listener: waiting to recvfrom...\n");

    int num_file = 0;
    std::string filename;
    unsigned char dbuf[PACKET_SIZE]; // buffer to recv data packets
    unsigned char abuf[PACKET_SIZE]; // buffer to send back acks
    unsigned char data[CHUNCK_SIZE];
    PacketHeader dheader; // header for data packets
    PacketHeader aheader; // hedaer for ACKs
    std::deque<DataPacket> slideWindow;
    unsigned int expSeqNum = 0; // expected seqnum;
    bool inConnection = false;  // in the middle of a connection
    std::ofstream outfile;
    std::ofstream logfile;
    logfile.open(log_dir);
    memset((unsigned char *)&sender_ip, 0, sizeof(sender_ip));
    // loop for recv data
    while (true)
    {

        memset(dbuf, '\0', PACKET_SIZE);

        numbytes = recvfrom(s, dbuf, PACKET_SIZE, 0, (struct sockaddr *)&si_other, &slen);
        if (numbytes == -1)
        {
            die("recvfrom");
        }

        memset(data, '\0', CHUNCK_SIZE);
        deserialize_packet(&dheader, dbuf, data);

        writeLog(logfile, &dheader);
        // logfile << dheader.type << '\t' << dheader.seqNum << '\t' << dheader.length << '\t' << dheader.checksum << std::endl;
        writeCMD(&dheader);
        // std::cout << dheader.type << '\t' << dheader.seqNum << '\t' << dheader.length << '\t' << dheader.checksum << std::endl;

        // check checksum
        bool correctChecksum = true;
        unsigned int checksum = crc32(data, dheader.length);
        if (dheader.checksum != checksum)
        {
            correctChecksum = false;
        }

        if (dheader.type == 0)
        { // START
            // if (sender_ip.sin_addr.s_addr == si_other.sin_addr.s_addr)
            // printf("!!!!!!!!!!!!!!!!!type 0 == tes\n");
            if (!inConnection)
            { // start a new connection
              // strcpy(sender_ip, their_ip);

                sender_ip.sin_addr.s_addr = si_other.sin_addr.s_addr;
                std::cout << sender_ip.sin_addr.s_addr << " " << si_other.sin_addr.s_addr << std::endl;
                // set ACK
                aheader.type = 3;
                aheader.seqNum = dheader.seqNum;
                aheader.length = 0;
                aheader.checksum = 0;

                std::cout << "-->";
                writeCMD(&aheader);

                // open new file

                outfile.open(output_dir + "/FILE-" + std::to_string(num_file) + ".out", std::ofstream::binary);
                std::cout << "file: "
                          << "." + output_dir + "/FILE-" + std::to_string(num_file) + ".out" << std::endl;
                if (!outfile)
                {
                    die("cannot open file");
                }

                inConnection = true;
            }
            else if (sender_ip.sin_addr.s_addr == si_other.sin_addr.s_addr)
            { // START from other ip, ignore
              //     // set ACK
               printf("!!!!!!!!!!!!!!!!type 0 == tes\n");
                aheader.type = 3;
                aheader.seqNum = dheader.seqNum;
                aheader.length = 0;
                aheader.checksum = 0;
            }
            else
            {
                continue;
            }
        }
        else if (dheader.type == 1)
        { // END
            aheader.type = 3;
            aheader.seqNum = dheader.seqNum;
            aheader.length = 0;
            aheader.checksum = 0;
            if (sender_ip.sin_addr.s_addr == si_other.sin_addr.s_addr)
            {

                printf("!!!!!!!!!!!!!!!!type 1 == tes\n");
                end_flag = false;
                expSeqNum = 0;
                inConnection = false;
                // num_file++;
                memset((unsigned char *)&sender_ip, 0, sizeof(sender_ip));
                outfile.close();
            }
        }
        else if (dheader.type == 2)
        {
            //
            if (sender_ip.sin_addr.s_addr != si_other.sin_addr.s_addr)
            {
                // std::cout << "nonono 2" << sender_ip.sin_addr.s_addr << " " << si_other.sin_addr.s_addr << std::endl;
                continue;
            }

            if (dheader.seqNum >= expSeqNum + stoi(window_size))
            {
                continue;
            }

            if (dheader.seqNum >= expSeqNum)
            { // if <, immediatley send back an ACK

                for (int i = slideWindow.size(); i <= dheader.seqNum - expSeqNum; i++)
                {
                    DataPacket dpack;
                    dpack.length = 0;
                    dpack.data = NULL;
                    slideWindow.push_back(dpack);
                }

                unsigned char *newData = new unsigned char[CHUNCK_SIZE];
                for (int i = 0; i < dheader.length; i++)
                {
                    newData[i] = data[i];
                }
                slideWindow[dheader.seqNum - expSeqNum].data = newData;
                slideWindow[dheader.seqNum - expSeqNum].length = dheader.length;

                if (dheader.seqNum == expSeqNum)
                {
                    while (!slideWindow.empty() && slideWindow[0].data != NULL)
                    {

                        outfile.write((char *)slideWindow[0].data, slideWindow[0].length);
                        delete[] slideWindow[0].data;
                        slideWindow.pop_front();
                        expSeqNum++;
                    }
                }
            }
            aheader.type = 3;
            aheader.seqNum = expSeqNum;
            aheader.length = 0;
            aheader.checksum = 0;
        }
        else
        {
            continue;
        }

        // send ACK
        memset(&abuf, '\0', PACKET_SIZE);
        serialize_header_to_unsigned_char(&aheader, abuf);
        numbytes = sendto(s, abuf, PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen);

        if (numbytes == -1)
        {
            die("fail send to ack");
        }
        writeLog(logfile, &dheader);
        // logfile << aheader.type << '\t' << aheader.seqNum << '\t' << aheader.length << '\t' << aheader.checksum << std::endl;
        if (DEBUG)
        {
            std::cout << "--> send to";
            writeCMD(&aheader);
        }

        if (end_flag)
        {
            break;
        }
    }

    return 0;
}