#include "pch.h"
#include "Socket.h"
#include <iostream>
#include <sstream>
#if __linux__
#include <fcntl.h> //fcntl
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#define INVALID_SOCKET          (~0)
#define SOCKET_ERROR            (-1)
#elif defined(WIN32)
#include <ws2tcpip.h>
#endif

Socket::Socket() :m_Handle(INVALID_SOCKET)
{
	
	
}

Socket::~Socket()
{
#ifdef WIN32
	closesocket(m_Handle);
#elif __linux__
	close(m_Handle);
#endif
}

Socket::Socket(Socket && other)
{
	this->m_Handle = other.m_Handle;
	other.m_Handle = INVALID_SOCKET;
}

Socket& Socket::operator=(Socket &&other)
{
	if (this == &other) return *this;
	this->m_Handle = other.m_Handle;
	other.m_Handle = INVALID_SOCKET;

	return *this;
}

bool Socket::Connect(std::string ip, std::size_t port)
{
	m_Handle = socket(AF_INET, SOCK_STREAM, IPPROTO_HOPOPTS);
	struct sockaddr_in remote;
	remote.sin_family = AF_INET;
	if (inet_pton(AF_INET, ip.c_str(), &(remote.sin_addr)) <= 0)
	{
		std::cerr << "[Socket] Can't set remote sin_addr";
		return 0;
	}

	remote.sin_port = htons(port);
	if (connect(m_Handle, (struct sockaddr *)&remote, sizeof(struct sockaddr)) < 0)
	{
		
		return 0;
	}

	

	return 1;
}

bool Socket::ConnectToHost(std::string host, std::size_t port)
{
	int iResult;
	std::stringstream ss;
	ss << port;

	//struct addrinfo *result = NULL,	*ptr = NULL;

	// // Resolve the server address and port
	// auto iResult = getaddrinfo(host.c_str(), ss.str().c_str(), &hints, &result);
	// if (iResult != 0) {
		
	// 	std::cout << "[Socket] Can't get address info: " << host << ":" << port << std::endl;
	// 	return 0;
	// }

	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in *h;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;

	if ( (rv = getaddrinfo( host.c_str() , "http" , &hints , &servinfo)) != 0) 
	{
		std::cout << "[Socket] Can't get address info: " << host << ":" << port << std::endl;
		return false;
	}

	bool r = false;
	for(auto ptr = servinfo; p != NULL; p = p->ai_next) 
	{
		m_Handle = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (m_Handle != INVALID_SOCKET)
		{
			iResult = connect(m_Handle, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (iResult == SOCKET_ERROR) {
#ifdef WIN32
				closesocket(m_Handle);
#elif __linux__
				close(m_Handle);
#endif
				r = false;
			}
			else
			{
				r = true;
				break;
			}
		}
	}

	freeaddrinfo(servinfo);
	
	return r;
}

bool Socket::Listen(std::size_t port)
{
	std::stringstream ss;
	ss << port;
	struct addrinfo *result = NULL, *ptr = NULL, hints;

	
#ifdef WIN32
	ZeroMemory(&hints, sizeof(hints));
#endif
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	// Resolve the local address and port to be used by the server
	auto iResult = getaddrinfo(NULL, ss.str().c_str(), &hints, &result);
	if (iResult != 0) {
		printf("[Socket] getaddrinfo failed: %d\n", iResult);
		
		return 0;
	}

	m_Handle = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (m_Handle == INVALID_SOCKET) {
		printf("[Socket] Error at socket()\n");
		return 0;
	}

	iResult = bind(m_Handle, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("[Socket] bind failed with error: %s\n",strerror(errno));
		freeaddrinfo(result);
		return 0;
	}

	if (listen(m_Handle, SOMAXCONN) == SOCKET_ERROR) {
		printf("[Socket] Listen failed with error\n");
	
		return 0;
	}


	return 1;
}

int Socket::Send(const std::string& buffer)
{
// 	int querylen = buffer.length();
// 	const char* p = buffer.c_str();
// 	int sent = 0;
// 	while (sent < querylen)
// 	{
// 		auto tmpres = send(m_Handle, p + sent, querylen - sent, 0);
// 		if (tmpres == -1)
// 		{
// #if __linux__
// 			printf("[Socket] Send Error: %s\n", strerror(errno));
// #elif defined(WIN32)
// 			int error = WSAGetLastError();
// 			printf("[Socket] Send Error: %d", error);
// #endif	
			
// 			return -1;
// 		}
// 		sent += tmpres;
// 	}

	int nsend = 0;
	nsend = send(m_Handle,buffer.data(),buffer.length(),0);

	if(nsend==-1)
	{
		printf("[Socket] Send Error: %s\n", strerror(errno));
	}
	return nsend;
}

int Socket::RecvAll(std::string & data,int timeout)
{
	static char data2[1924*1024*10];
	memset(data2, 0, sizeof(data2));

	char* p = data2;
	int size_recv, total_size = 0;
	
	char chunk[BUFSIZ];

#if __linux__
	struct timeval begin, now;
	double timediff;
	//make socket non blocking
	fcntl(m_Handle, F_SETFL, O_NONBLOCK);
	//beginning time
	gettimeofday(&begin, NULL);
#elif defined(WIN32)
	DWORD begin, now;
	DWORD timediff;
	u_long iMode = 1;
	ioctlsocket(m_Handle, FIONBIO, &iMode);
	begin = GetTickCount();
#endif
	//loop
	
	while (1)
	{
#if __linux__
		gettimeofday(&now, NULL);
		//time elapsed in seconds
		timediff = (now.tv_sec - begin.tv_sec) + 1e-6 * (now.tv_usec - begin.tv_usec);
#elif defined(WIN32)
		now = GetTickCount();
		//time elapsed in seconds
		timediff = (now - begin)*0.001f;
#endif

		//if you got some data, then break after timeout
		if (total_size > 0 && timediff > timeout)
		{
			break;
		}

		//if you got no data at all, wait a little longer, twice the timeout
		else if (timediff > timeout * 2)
		{
			break;
		}

		memset(chunk, 0, BUFSIZ);  //clear the variable
		if ((size_recv = recv(m_Handle, chunk, BUFSIZ, 0)) < 0)
		{
#if __linux__
			usleep(100000);
#elif defined(WIN32)
			Sleep(100);
#endif
		}
		else
		{
			total_size += size_recv;
			//printf("%s", chunk);
			//data2.append(chunk);
			memcpy(p, chunk, size_recv);
			p += size_recv;
			//reset beginning time
#if __linux__
			gettimeofday(&now, NULL);
#elif defined(WIN32)
			now = GetTickCount();
#endif
		}
	}

	data.resize(total_size);
	memcpy(&data[0], data2, total_size);
	return total_size;
}

int Socket::Recv(std::string & data)
{
	
	int size_recv;
	char chunk[BUFSIZ];
	size_recv = recv(m_Handle, chunk, BUFSIZ, 0);
	if (size_recv > 0)	data.assign(chunk,size_recv);
	

	return size_recv;
}

Socket Socket::Accept(sockaddr_in* remoteaddr)
{
	

	auto ClientSocket = INVALID_SOCKET;

	// Accept a client socket
#if defined(WIN32)
	int addrlen = sizeof(sockaddr_in);
	ClientSocket = accept(m_Handle, (struct sockaddr *)remoteaddr, &addrlen);
#elif defined(__linux__)
	int addrlen = sizeof(sockaddr);
	ClientSocket = accept(m_Handle, (struct sockaddr *)remoteaddr, (socklen_t*)&addrlen);
#endif
	if (ClientSocket == INVALID_SOCKET)
	{
		return Socket();	
	}

	return std::move(Socket(ClientSocket));
}

bool Socket::IsValid()
{
	return m_Handle!=INVALID_SOCKET;
}

int Socket::SetOpt(int level, int optname, const char * optval, int optlen)
{
	return setsockopt(m_Handle, level, optname, optval, optlen);
}

void Socket::EnableNonblock()
{
#if __linux__
	int flags = fcntl(m_Handle, F_GETFL, 0);
	fcntl(m_Handle, F_SETFL, flags|O_NONBLOCK);
#elif defined(WIN32)
	u_long iMode = 1;
	ioctlsocket(m_Handle, FIONBIO, &iMode);
#endif
}
Socket::Socket(int handle):m_Handle(handle)
{
}

int Socket::GetHandle()
{
	return m_Handle;
}
