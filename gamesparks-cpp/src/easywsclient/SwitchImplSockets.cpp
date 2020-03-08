#include "GameSparks/GSPlatformDeduction.h"
#include "easywsclient/easywsclient.hpp"

#if (GS_TARGET_PLATFORM == GS_PLATFORM_NINTENDO_SDK)

#include <nn/os.h>
#include <nn/init.h>
#include <nn/nn_Log.h>
#include <nn/nn_Assert.h>
#include <nn/lmem/lmem_ExpHeap.h>
#include <nn/nifm.h>
#include <nn/nifm/nifm_ApiIpAddress.h>
#include <nn/socket.h>
#include <nn/ssl.h>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <string.h>
#include "GameSparks/GSLeakDetector.h"
#include "GameSparks/GSUtil.h"


class BaseSocket;

class TCPSocket : public BaseSocket
{
private:
	bool is_connected;
	volatile bool is_aborted;

protected:
	bool is_blocking;
	int socketDescriptor;

public:
	TCPSocket() : is_connected(false), is_aborted(false), is_blocking(true), socketDescriptor(-1)
	{
	}

	virtual ~TCPSocket()
	{
		close();
	}

	virtual bool connect(const char *host, short port)
	{
		char ip[64] = {'\0'};
		int rc = -1;
		in_addr outAddr = {0};
		sockaddr_in saServer = {0};
		struct addrinfo hints = {'\0'};
		struct addrinfo *pAddressInfoResult = NULL;
		struct sockaddr_in *pSin = NULL;

		if (is_connected) {
			return true;
		}

		if ((socketDescriptor = nn::socket::Socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			set_errstr("Failed to create TCP socket (errno: %d)", errno);

			return false;
		}

		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		if ((rc = nn::socket::GetAddrInfo(host, NULL, &hints, &pAddressInfoResult)) != 0)
		{
			set_errstr("Failed to resolve host name (errno:%d)", errno);

			close();

			return false;
		}

		if (!pAddressInfoResult) {
			set_errstr("pAddressInfoResult is NULL!");

			close();

			return false;
		}

		for (struct addrinfo* pAddrinfo = pAddressInfoResult; pAddrinfo != NULL; pAddrinfo = pAddrinfo->ai_next) {
			if (AF_INET == pAddrinfo->ai_family) {
				pSin = reinterpret_cast<struct sockaddr_in*>(pAddrinfo->ai_addr);
				if (pSin) {
					strcpy(ip, nn::socket::InetNtoa(pSin->sin_addr));
				}

				break;
			}
		}

		if (pAddressInfoResult) {
			nn::socket::FreeAddrInfo(pAddressInfoResult);
		}

		nn::socket::InetAton(ip, &outAddr);

		saServer.sin_addr = outAddr;
		saServer.sin_port = nn::socket::InetHtons(static_cast<uint16_t>(port));
		saServer.sin_family = AF_INET;

		if (nn::socket::Connect(socketDescriptor, reinterpret_cast<sockaddr *>(&saServer), sizeof(saServer)) < 0)
		{
			set_errstr("Failed to connect TCP socket %d (errno: %d).", socketDescriptor, errno);

			close();

			return false;
		}

		if (is_aborted) {
			set_errstr("Connection aborted!");

			return false;
		}

		//NN_LOG("Established TCP connection (addr:0x%x on port :%d).\n", saServer.sin_addr.s_addr, nn::socket::InetNtohs(saServer.sin_port));

		is_connected = true;

		return true;
	}

	virtual void close()
	{
		is_connected = false;

		if (socketDescriptor >= 0) {
			nn::socket::Close(socketDescriptor);
			socketDescriptor = -1;
		}
	}

	virtual int send(const char *buf, size_t siz)
	{
		if (is_aborted) {
			return -1;
		}

		return nn::socket::Send(socketDescriptor, buf, siz, is_blocking ? nn::socket::MsgFlag::Msg_None : nn::socket::MsgFlag::Msg_DontWait);
	}

	virtual int recv(char *buf, size_t siz)
	{
		if (is_aborted) {
			return -1;
		}

		return nn::socket::Recv(socketDescriptor, buf, siz, is_blocking ? nn::socket::MsgFlag::Msg_None : nn::socket::MsgFlag::Msg_DontWait);
	}

	virtual void set_blocking(bool should_block)
	{
		assert(is_connected);

		if (!is_connected || is_aborted) {
			return;
		}
	
		is_blocking = should_block;
	}

	virtual void abort()
	{
		is_aborted = true;
		
		close();
	}
};

class TLSSocket : public TCPSocket
{
private:
	static const uint32_t ReadBufferSize = 1024 * 512;
	static const uint32_t MaxAsyncDoHandshakeRetry = 30;
	static const uint32_t MsecPollTimeout = 5000;

	bool is_connected;
	//static bool is_sslInitialised;
	char *m_pReceivedData;
	//uint32_t m_ReceivedTotalBytes;
	nn::ssl::Context m_SslContext;
	nn::ssl::Connection m_SslConnection;

	static void debug_print(void *ctx, int level, const char *file, int line, const char *str)
	{
		((void)level);
		fprintf((FILE *)ctx, "%s:%04d: %s", file, line, str);
		//fflush((FILE *)ctx);
	}

	bool ConfigureHostName(const char *host)
	{
		bool isSuccess = true;
		uint32_t hostNameLen = static_cast<uint32_t>(strlen(host));
		nn::Result result = m_SslConnection.SetHostName(host, hostNameLen);

		if (result.IsFailure()) {
			set_errstr("m_SslConnection.SetHostName failed (Description:%d)", result.GetDescription());

			return false;
		}

		char *pOutHostname = new char[hostNameLen + 1];

		if (pOutHostname == nullptr) {
			set_errstr("Failed to allocate hostname buffer.");

			return false;
		} else {
			uint32_t outHostNameLen = 0;
			result = m_SslConnection.GetHostName(pOutHostname, &outHostNameLen, hostNameLen + 1);

			if (result.IsFailure()) {
				set_errstr("m_SslConnection.GetHostName failed (Description:%d)", result.GetDescription());

				isSuccess = false;
			} else {
				/*if (outHostNameLen > 0) {
					NN_LOG("HostNameLen: %u, HostName: %s\n", outHostNameLen, pOutHostname);
				} else {
					NN_LOG("HostNameLen: %u\n", outHostNameLen);
				}*/
			}
			
			delete[] pOutHostname;
		}

		return isSuccess;
	}

	bool ConfigureVerifyOption(nn::ssl::Connection::VerifyOption verifyOption)
	{
		nn::Result result = m_SslConnection.SetVerifyOption(verifyOption);

		if (result.IsFailure()) {
			set_errstr("m_SslConnection.SetVerifyOption failed (Description:%d)", result.GetDescription());

			return false;
		}

		return true;
	}

	bool ConfigureIoMode()
	{
		nn::Result result = nn::ResultSuccess();

		result = m_SslConnection.SetIoMode(is_blocking ? nn::ssl::Connection::IoMode_Blocking : nn::ssl::Connection::IoMode_NonBlocking);
		if (result.IsFailure()) {
			set_errstr("Failed to configure I/O blocking mode (Description:%d)", result.GetDescription());

			return false;
		}

		return true;
	}

	bool PerformSslHandshake()
	{
		nn::Result result = nn::ResultSuccess();
		bool isSuccess = true;
		char *serverCertBuff = nullptr;
		uint32_t asyncHandshakeCount = 0;
		uint32_t serverCertLength = 0;

		do {
			result = (serverCertBuff) ? m_SslConnection.DoHandshake(&serverCertLength, nullptr) : m_SslConnection.DoHandshake();
			if (result.IsSuccess()) {
				//NN_LOG("SSL Handshake completed.\n");

				break;
			}

			if (nn::ssl::ResultBufferTooShort::Includes(result)) {
				uint32_t neededBufSize = 0;
				nn::Result tempResult = m_SslConnection.GetNeededServerCertBufferSize(&neededBufSize);

				if (tempResult.IsFailure())
				{
					set_errstr("Error: GetNeededServerCertBufferSize() failed with desc: %d", tempResult.GetDescription());

					isSuccess = false;

					break;
				}

				set_errstr("Provided buffer was not long enough to store server cert. Required buffer length: %u", neededBufSize);
			} else if (nn::ssl::ResultIoWouldBlock::Includes(result)) {
				if (asyncHandshakeCount++ < this->MaxAsyncDoHandshakeRetry)
				{
					set_errstr("Handshake would block, try again after 100 msec (retry count:%d)", asyncHandshakeCount);

					nn::os::SleepThread(nn::TimeSpan::FromMilliSeconds(100));

					continue;
				}
				else
				{
					set_errstr("Handshake failed after %d times of execution.", asyncHandshakeCount);

					break;
				}
			} else {
				set_errstr("SSL Handshake failed (Description:%d)", result.GetDescription());

				if (nn::ssl::ResultVerifyCertFailed::Includes(result)) {
					isSuccess = false;
				}

				break;
			}
		} while (true);

		if (serverCertBuff != nullptr) {
			delete[] serverCertBuff;
		}

		return isSuccess;
	}

public:
	TLSSocket() : is_connected(false), m_pReceivedData(nullptr)//, m_ReceivedTotalBytes(0)
	{
		/*nn::Result result = nn::ResultSuccess();

		if (!is_sslInitialised) {
			result = nn::ssl::Initialize();
			if (result.IsFailure()) {
				NN_LOG("Failed to initialize SSL library (errno: %d)\n", errno);

				return;
			}

			is_sslInitialised = true;
		}*/
	}

	/*virtual ~TLSSocket()
	{
		if (is_sslInitialised) {
			//nn::ssl::Finalize();
			is_sslInitialised = false;
		}
	}*/

	virtual bool connect(const char *host, short port)
	{
		nn::Result result = nn::ResultSuccess();
		nn::ssl::SslContextId contextId;

		if (is_connected) {
			return true;
		}

		if (!TCPSocket::connect(host, port)) {
			return false;
		}

		m_pReceivedData = new char[ReadBufferSize];
		if (!m_pReceivedData) {
			set_errstr("Failed to allocate memory for read buffer.");

			return false;
		}
		memset(m_pReceivedData, 0x00, ReadBufferSize);

		result = m_SslContext.Create(nn::ssl::Context::SslVersion::SslVersion_Auto);
		if (result.IsFailure()) {
			set_errstr("Failed to create context (Description:%d)", result.GetDescription());

			delete[] m_pReceivedData;
			m_pReceivedData = nullptr;

			return false;
		}

		result = m_SslContext.GetContextId(&contextId);
		if (result.IsFailure()) {
			set_errstr("Failed to get context ID (Description:%d)", result.GetDescription());

			delete[] m_pReceivedData;
			m_pReceivedData = nullptr;

			return false;
		}
		
		result = m_SslConnection.Create(&m_SslContext);
		if (result.IsFailure()) {
			set_errstr("m_SslConnection.Create failed (Description:%d)", result.GetDescription());

			delete[] m_pReceivedData;
			m_pReceivedData = nullptr;

			return false;
		}

		result = m_SslConnection.SetSocketDescriptor(socketDescriptor);
		if (result.IsFailure()) {
			set_errstr("m_SslConnection.SetSocketDescriptor failed (Description:%d)", result.GetDescription());

			delete[] m_pReceivedData;
			m_pReceivedData = nullptr;

			return false;
		}	

		if (!ConfigureHostName(host) || !ConfigureVerifyOption(nn::ssl::Connection::VerifyOption::VerifyOption_All) || !ConfigureIoMode() || !PerformSslHandshake()) {
			//NN_LOG("Something wrong!\n");
			
			delete[] m_pReceivedData;
			m_pReceivedData = nullptr;

			return false;
		}

		is_connected = true;

		return true;
	}

	virtual void close()
	{
		nn::ssl::SslConnectionId connectiontId;
		nn::ssl::SslContextId contextId;

		//TCPSocket::close();

		if (m_SslConnection.GetConnectionId(&connectiontId).IsSuccess()) {
			if (connectiontId) {
				m_SslConnection.Destroy();
			}
		}

		if (m_SslContext.GetContextId(&contextId).IsSuccess()) {
			if (contextId != 0) {
				m_SslContext.Destroy();
			}
		}

		if (m_pReceivedData != nullptr) {
			delete[] m_pReceivedData;
			m_pReceivedData = nullptr;
		}

		is_connected = false;
	}

	virtual int send(const char *buf, size_t siz)
	{
		return m_SslConnection.Write(buf, siz);
	}

	virtual int recv(char *buf, size_t siz)
	{
		return m_SslConnection.Read(buf, siz);
	}

	virtual void set_blocking(bool should_block)
	{
		TCPSocket::set_blocking(should_block);

		ConfigureIoMode();
	}
};

//bool TLSSocket::is_sslInitialised = false;

BaseSocket *BaseSocket::create(bool useSSL) {
	if (useSSL) {
		return new TLSSocket();
	} else {
		return new TCPSocket();
	}
}

#endif
