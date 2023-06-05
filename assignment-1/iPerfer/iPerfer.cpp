#include <arpa/inet.h>	// htons()
#include <stdio.h>		// printf(), perror()
#include <stdlib.h>		// atoi()
#include <sys/socket.h> // socket(), bind(), listen(), accept(), send(), recv()
#include <unistd.h>		// close()

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
// make_server_sockaddr(), get_port_number()
using namespace std;
static const size_t MAX_MESSAGE_SIZE = 256;
#define MAXDATASIZE 1000 // max number of bytes we can get at once
#define MAXBUFLEN 1000

#include <arpa/inet.h>	// htons(), ntohs()
#include <netdb.h>		// gethostbyname(), struct hostent
#include <netinet/in.h> // struct sockaddr_in
#include <stdio.h>		// perror(), fprintf()
#include <string.h>		// memcpy()
#include <sys/socket.h> // getsockname()
#include <unistd.h>		// stderr

/**
 * Make a server sockaddr given a port.
 * Parameters:
 *		addr: 	The sockaddr to modify (this is a C-style function).
 *		port: 	The port on which to listen for incoming connections.
 * Returns:
 *		0 on success, -1 on failure.
 * Example:
 *		struct sockaddr_in server;
 *		int err = make_server_sockaddr(&server, 8888);
 */
int make_server_sockaddr(struct sockaddr_in *addr, int port)
{
	// Step (1): specify socket family.
	// This is an internet socket.
	addr->sin_family = AF_INET;

	// Step (2): specify socket address (hostname).
	// The socket will be a server, so it will only be listening.
	// Let the OS map it to the correct address.
	addr->sin_addr.s_addr = INADDR_ANY;

	// Step (3): Set the port value.
	// If port is 0, the OS will choose the port for us.
	// Use htons to convert from local byte order to network byte order.
	addr->sin_port = htons(port);

	return 0;
}

/**
 * Make a client sockaddr given a remote hostname and port.
 * Parameters:
 *		addr: 		The sockaddr to modify (this is a C-style function).
 *		hostname: 	The hostname of the remote host to connect to.
 *		port: 		The port to use to connect to the remote hostname.
 * Returns:
 *		0 on success, -1 on failure.
 * Example:
 *		struct sockaddr_in client;
 *		int err = make_client_sockaddr(&client, "141.88.27.42", 8888);
 */
int make_client_sockaddr(struct sockaddr_in *addr, const char *hostname, int port)
{
	// Step (1): specify socket family.
	// This is an internet socket.
	addr->sin_family = AF_INET;

	// Step (2): specify socket address (hostname).
	// The socket will be a client, so call this unix helper function
	// to convert a hostname string to a useable `hostent` struct.
	struct hostent *host = gethostbyname(hostname);
	if (host == nullptr)
	{
		fprintf(stderr, "%s: unknown host\n", hostname);
		return -1;
	}
	memcpy(&(addr->sin_addr), host->h_addr, host->h_length);

	// Step (3): Set the port value.
	// Use htons to convert from local byte order to network byte order.
	addr->sin_port = htons(port);

	return 0;
}

/**
 * Return the port number assigned to a socket.
 *
 * Parameters:
 * 		sockfd:	File descriptor of a socket
 *
 * Returns:
 *		The port number of the socket, or -1 on failure.
 */
int get_port_number(int sockfd)
{
	struct sockaddr_in addr;
	socklen_t length = sizeof(addr);
	if (getsockname(sockfd, (sockaddr *)&addr, &length) == -1)
	{
		perror("Error getting port of socket");
		return -1;
	}
	// Use ntohs to convert from network byte order to host byte order.
	return ntohs(addr.sin_port);
}

/**
 * Receives a string message from the client and prints it to stdout.
 *
 * Parameters:
 * 		connectionfd: 	File descriptor for a socket connection
 * 				(e.g. the one returned by accept())
 * Returns:
 *		0 on success, -1 on failure.
 */
int handle_connection(int connectionfd)
{

	// printf("New connection %d\n", connectionfd);

	// (1) Receive message from client.

	char buffer[1000];
	memset(buffer, 0, sizeof(buffer));
	// Call recv() enough times to consume all the data the client sends.
	size_t recvd = 0;
	ssize_t rval;

	char ack_b[256];
	bzero(ack_b, 256);

	// time_t start_t ;
	// start_t = time(NULL);

	struct timeval st1, st2;
	gettimeofday(&st1, NULL);
	// printf("Starting clock , start_t: %ld \n" , start_t);
	long long total_receive = 0;

	do
	{
		// Receive as many additional bytes as we can in one call to recv()
		// (while not exceeding MAX_MESSAGE_SIZE bytes in total).
		rval = recv(connectionfd, buffer, sizeof(buffer), MSG_WAITALL);
		total_receive += 1;
		if (rval == -1)
		{
			perror("Error reading stream message");
			return -1;
		}
		recvd += rval;
	} while (rval > 0); // recv() returns 0 when client closes

	gettimeofday(&st2, NULL);
	/*	time_t end_times ;

	// (2) Print out the message
	// printf("Client %d says , %ld\n", connectionfd, total_receive);

	// printf("Starting clock , end_time: %ld \n" , end_times);

	double total_time_receive = (st2.tv_sec - st1.tv_sec);
	total_time_receive += (st2.tv_usec - st1.tv_usec) / 1000000.0;
	printf("total_time_receive: %f\n", total_time_receive);
	// cout << "total_time_receive: "  << total_time_receive << endl;
	long long sent = total_receive - 1;
	long long sent_mb = sent * 0.001;
	long long sent_bit = sent_mb * 8;

	double rate;
	rate = 8 * sent_bit / (total_time_receive * 10);
	printf("Received=%lld KB, Rate=%.3f Mbps", sent, rate);
	/*	time_t total_time_receive = (end_times - start_t);
		printf(">>>total time: %ld\n",total_time_receive );

			long long sent = total_receive -1;


			long long sent_mb = sent * 0.001;
			long long sent_bit = sent_mb*8;
			double tmp =   sent_bit /  (total_time_receive);
		//	printf("total_time_receive: %f:" ,total_time_receive);
			double rate;
			rate=8*sent_bit/(total_time_receive * 10);
			printf("Received=%lld KB, Rate=%.3f Mbps", sent , rate);
	*/

	// (4) Close connection

	close(connectionfd);

	return -1;
}

/**
 * Endlessly runs a server that listens for connections and serves
 * them _synchronously_.
 *
 * Parameters:
 *		port: 		The port on which to listen for incoming connections.
 *		queue_size: 	Size of the listen() queue
 * Returns:
 *		-1 on failure, does not return on success.
 */
int run_server(int port, int queue_size)
{

	// (1) Create socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		perror("Error opening stream socket");
		return -1;
	}

	// (2) Set the "reuse port" socket option
	int yesval = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yesval, sizeof(yesval)) == -1)
	{
		perror("Error setting socket options");
		return -1;
	}

	// (3) Create a sockaddr_in struct for the proper port and bind() to it.
	struct sockaddr_in addr;
	if (make_server_sockaddr(&addr, port) == -1)
	{
		return -1;
	}

	// (3b) Bind to the port.
	if (bind(sockfd, (sockaddr *)&addr, sizeof(addr)) == -1)
	{
		perror("Error binding stream socket");
		return -1;
	}

	// (3c) Detect which port was chosen.
	port = get_port_number(sockfd);
	// printf("Server listening on port %d...\n", port);

	// (4) Begin listening for incoming connections.
	listen(sockfd, queue_size);

	// (5) Serve incoming connections one by one forever.
	while (true)
	{
		int connectionfd = accept(sockfd, 0, 0);
		if (connectionfd == -1)
		{
			perror("Error accepting connection");
			return -1;
		}

		if (handle_connection(connectionfd) == -1)
		{
			return -1;
		}
	}
}

//^^^^^^ server

int send_message(const char *hostname, int port, const char *message, time_t duration)
{

	if (strlen(message) > MAX_MESSAGE_SIZE)
	{
		perror("Error: Message exceeds maximum length\n");
		return -1;
	}

	// (1) Create a socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// (2) Create a sockaddr_in to specify remote host and port
	struct sockaddr_in addr;
	if (make_client_sockaddr(&addr, hostname, port) == -1)
	{
		return -1;
	}

	// (3) Connect to remote server
	if (connect(sockfd, (sockaddr *)&addr, sizeof(addr)) == -1)
	{
		perror("Error connecting stream socket");
		return -1;
	}

	// (4) Send message to remote server
	// printf("send_message time: %ld\n" , duration);
	time_t start_t, total_t;
	start_t = time(NULL);
	// printf("Starting clock , start_t: %ld \n" , start_t);
	time_t end_time = start_t + duration;

	// printf("clock() : %ld , end_time: %ld" , start_t , end_time);

	char cbuffer[256];
	bzero(cbuffer, 256);
	int cblen = 256;

	char buf[1000];
	memset(buf, '0', 1000);
	buf[1000] = '\0';
	// printf("%s\n" , buf);
	int len = strlen(buf);
	// printf("len: %d \n" , len);

	long long total_sent = 0;
	int n;
	while (time(NULL) <= end_time)
	{

		// printf("%ld\n" , (end_time - time(NULL)  ) );
		if (n = send(sockfd, buf, len, 0) == -1)
		{
			perror("Error sending on stream socket");
			return -1;
		}
		total_sent += 1;
	}

	total_t = time(NULL);
	double realTime = (total_t - start_t);

	long long sent = total_sent * 0.001;
	double rate;
	rate = 8 * total_sent / (realTime * 1000);
	printf("Sent=%lld KB, Rate=%.3f Mbps\n", total_sent, rate);

	// (5) Close connection
	close(sockfd);

	return 0;
}

// ^^^^client

int main(int argc, char *argv[])
{
	// const regex reg("^(-s -p (-?)[0-9]+");
	if (argc == 4)
	{
		// printf("this is argc 4\n");
		if (strcmp(argv[1], "-s") == 0 && strcmp(argv[2], "-p") == 0)
		{
			int listen_port = atoi(argv[3]);
			if (listen_port >= 1024 && listen_port <= 65535)
			{
				if (run_server(listen_port, 10) == -1)
				{
					return 1;
				}
			}
			else
			{
				printf("Error: port number must be in the range of [1024, 65535]\n");
				return 1;
			}
		}
		else
		{
			printf("Error: missing or extra arguments\n");
			return 1;
		}
	}
	else if (argc == 8)
	{
		// printf("this is argc 8\n");
		if (strcmp(argv[1], "-c") == 0 && strcmp(argv[2], "-h") == 0 && strcmp(argv[4], "-p") == 0 && strcmp(argv[6], "-t") == 0)
		{
			const char *hostname = argv[3];
			int server_port = atoi(argv[5]);
			if (server_port >= 1024 && server_port <= 65535)
			{
				time_t time = atoi(argv[7]);
				if (time > 0)
				{
					const char *message = "testing";
					// printf("Sending message %s to %s:%d\n", message, hostname, server_port);
					if (send_message(hostname, server_port, message, time) == -1)
					{
						return 1;
					}
				}
				else
				{
					printf("Error: time argument must be greater than 0");
					return 1;
				}
			}
			else
			{
				printf("Error: port number must be in the range of [1024, 65535]\n");
				return 1;
			}
		}
		else
		{
			printf("Error: missing or extra arguments\n");
			return 1;
		}
	}
	else
	{
		printf("Error: missing or extra arguments\n");
		return 1;
	}

	return 0;
}
