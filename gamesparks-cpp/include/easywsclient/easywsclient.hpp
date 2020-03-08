#ifndef EASYWSCLIENT_HPP_20120819_MIOFVASDTNUASZDQPLFD
#define EASYWSCLIENT_HPP_20120819_MIOFVASDTNUASZDQPLFD

// This code comes from:
// https://github.com/dhbaird/easywsclient
//
// To get the latest version:
// wget https://raw.github.com/dhbaird/easywsclient/master/easywsclient.hpp
// wget https://raw.github.com/dhbaird/easywsclient/master/easywsclient.cpp

#include <GameSparks/gsstl.h>

#if (GS_TARGET_PLATFORM == GS_PLATFORM_WIN32 || GS_TARGET_PLATFORM == GS_PLATFORM_PS4 || GS_TARGET_PLATFORM == GS_PLATFORM_ANDROID)
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/net.h>
#include <mbedtls/error.h>
#include <mbedtls/platform.h>
#include <mbedtls/debug.h>
#endif

class BaseSocket
{
	gsstl::string error_string;

protected:
	BaseSocket()
	{
		// reserve space for the error message, because marmalades memory manager is not threwad-safe
		error_string.reserve(512);
	}

	void set_errstr(int res)
	{
		char buf[256];
#if (GS_TARGET_PLATFORM == GS_PLATFORM_WIN32 || GS_TARGET_PLATFORM == GS_PLATFORM_PS4 || GS_TARGET_PLATFORM == GS_PLATFORM_ANDROID)
		mbedtls_strerror(res, buf, sizeof(buf));
#elif (GS_TARGET_PLATFORM == GS_PLATFORM_NINTENDO_SDK)
		sprintf(buf, "error %x", res);
#elif (GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC)
        sprintf(buf, "error %x", res);
#else
		sprintf(buf, "error %d", res);
#endif
		error_string = buf;
	}

	void set_errstr(gsstl::string e)
	{
		error_string = e;
	}

#if (GS_TARGET_PLATFORM == GS_PLATFORM_NINTENDO_SDK)
	void set_errstr(const char* format, ...)
	{
		char buf[256];

		va_list argptr;
		va_start(argptr, format);
		sprintf(buf, format, argptr);
		va_end(argptr);

		error_string = buf;
	}
#endif		
public:
	virtual bool connect(const char *host, short port) = 0;
	virtual void close() = 0;
	virtual void set_blocking(bool should_block) = 0;

	virtual int send(const char *buf, size_t siz) = 0;
	virtual int recv(char *buf, size_t siz) = 0;

	virtual void abort() = 0;

	gsstl::string get_error_string() { return error_string; }
	virtual ~BaseSocket() {}

	static BaseSocket *create(bool useSSL);
};

namespace easywsclient {
	/* Used to pass Errors to the error callback */
	struct WSError
	{
		enum Code
		{
			ALL_OK, ///< all is good
			CONNECT_FAILED, ///< connecting to the remote failed, reasons might be DNS-Lookup failure, connect() failed or certificate verification failed
			UNEXPECTED_MESSAGE, ///< The server sent an unexpected message
			RECV_FAILED, ///< recv or SSL_read returned a nagative value
			CONNECTION_CLOSED, ///< the connection was closed - i.e. recv or SSL_read returned zero. Not necessarily an error
			SEND_FAILED, ///< send or SSL_write returned a nagative value
			CLOSED_DURING_WS_HANDSHAKE, ///< recv or SSL_read returned 0 during the websocket handshake
			INVALID_STATUS_LINE_DURING_WS_HANDSHAKE, ///< the status line received from the server during the websocket handshake was to long to fit into the buffer
			BAD_STATUS_CODE, ///< the HTTP status code returned was not 101 (Switching Protocols)
			DNS_LOOKUP_FAILED
		};

		WSError() : code(ALL_OK), message("") {}
		WSError(Code code_, const gsstl::string& message_) : code(code_), message(message_){}

		Code code; ///< one of the error codes
		gsstl::string message; ///< a more or less human readable error description
	};

	class WebSocket
	{
		public:
		typedef void (*WSMessageCallback)(const gsstl::string &, void*);
		typedef void(*WSErrorCallback)(const WSError&, void*);
		typedef WebSocket * pointer;
		typedef enum readyStateValues { CLOSING, CLOSED, CONNECTING, OPEN } readyStateValues;

		// Factories:
		static pointer create_dummy();
		static pointer from_url(const gsstl::string& url, const gsstl::string& origin = gsstl::string());
		static pointer from_url_no_mask(const gsstl::string& url, const gsstl::string& origin = gsstl::string());

		// only available on windows (when the MessageWebSocket is used)
		static pointer from_url_binary(const gsstl::string& url, const gsstl::string& origin = gsstl::string());

		// Interfaces:
		virtual ~WebSocket() { }
		virtual void poll(int timeout, WSErrorCallback errorCallback, void* userData) = 0; // timeout in milliseconds
		virtual void send(const gsstl::string& message) = 0;
		virtual void sendPing() = 0;
		virtual void close() = 0;
		virtual readyStateValues getReadyState() const = 0;

		void dispatch(WSMessageCallback messageCallback, WSErrorCallback errorCallback, void* userData) {
			_dispatch(messageCallback, errorCallback, userData);
		}
        
	protected:
		//struct Callback { virtual void operator()(const gsstl::string& message) = 0; };
		virtual void _dispatch(WSMessageCallback message_callback, WSErrorCallback error_callback, void* data) = 0;
        
    protected:
        enum dnsLookup
        {
            keNone,
            keInprogress,
            keComplete,
            keFailed
        };

        volatile dnsLookup ipLookup;
		WSError ipLookupError; // hold the error that might occur in the connect/dns thread
        
        gsstl::string m_host;
        gsstl::string m_path;
        gsstl::string m_url;
        gsstl::string m_origin;
        
        int m_port;
    };

} // namespace easywsclient

#endif /* EASYWSCLIENT_HPP_20120819_MIOFVASDTNUASZDQPLFD */
