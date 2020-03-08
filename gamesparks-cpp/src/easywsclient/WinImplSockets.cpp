#include "GameSparks/GSPlatformDeduction.h"
#include "easywsclient/easywsclient.hpp"

#if (GS_TARGET_PLATFORM == GS_PLATFORM_WIN32) && !(((GS_TARGET_PLATFORM == GS_PLATFORM_WIN32) && !GS_WINDOWS_DESKTOP) || GS_TARGET_PLATFORM == GS_PLATFORM_UWP || GS_TARGET_PLATFORM == GS_PLATFORM_XBOXONE)

#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/net.h>
#include <mbedtls/error.h>
#include <mbedtls/platform.h>
#include <mbedtls/debug.h>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <string.h>
#include "GameSparks/GSLeakDetector.h"
#include "GameSparks/GSUtil.h"
#include "easywsclient/CertificateStore.hpp"

#if defined(WIN32) && !defined(snprintf)
#   define snprintf _snprintf_s
#endif

class BaseSocket;

class TCPSocket : public BaseSocket
{
private:
	bool is_connected;
	volatile bool is_aborted;
protected:
	mbedtls_net_context net;
public:
	TCPSocket() :is_connected(false), is_aborted(false)
	{
		mbedtls_net_init(&net);
	}

	virtual ~TCPSocket()
	{
		mbedtls_net_free(&net);
	}

	virtual bool connect(const char *host, short port)
	{
		if (is_connected)
			return true;

		char port_str[8];
		snprintf(port_str, 8, "%hu", port);

		int res = mbedtls_net_connect(&net, host, port_str, MBEDTLS_NET_PROTO_TCP);

		if (res != 0)
		{
			set_errstr(res);
			close();
			return false;
		}

		if (is_aborted)
		{
			set_errstr("connection aborted!");
			return false;
		}

		is_connected = true;
		return true;
	}

	virtual void close()
	{
		is_connected = false;
	}

	virtual int send(const char *buf, size_t siz)
	{
		if (is_aborted) return MBEDTLS_ERR_NET_INVALID_CONTEXT;
		return mbedtls_net_send(&net, (unsigned char *)buf, siz);
	}

	virtual int recv(char *buf, size_t siz)
	{
		if (is_aborted) return MBEDTLS_ERR_NET_INVALID_CONTEXT;
		return mbedtls_net_recv(&net, (unsigned char *)buf, siz);
	}

	virtual void set_blocking(bool should_block)
	{
		assert(is_connected);

		if (!is_connected || is_aborted)
			return;

		if (should_block)
			mbedtls_net_set_block(&net);
		else
			mbedtls_net_set_nonblock(&net);
	}

	virtual void abort()
	{
		is_aborted = true;
		is_connected = false;
		mbedtls_net_free(&net);
		mbedtls_net_init(&net);
	}
};


class TLSSocket : public TCPSocket
{
private:
	bool is_connected;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;

	static void debug_print(void *ctx, int level, const char *file, int line, const char *str)
	{
		((void)level);
		mbedtls_fprintf((FILE *)ctx, "%s:%04d: %s", file, line, str);
		//fflush((FILE *)ctx);
	}
public:
	TLSSocket() :is_connected(false)
	{
		//mbedtls_debug_set_threshold( 1000 );
		mbedtls_ssl_init(&ssl);
		mbedtls_ssl_config_init(&conf);
		mbedtls_ctr_drbg_init(&ctr_drbg);
		mbedtls_entropy_init(&entropy);
	}

	virtual ~TLSSocket()
	{
		mbedtls_ssl_free(&ssl);
		mbedtls_ssl_config_free(&conf);
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);
	}

	virtual bool connect(const char *host, short port)
	{
		if (is_connected)
			return true;

		if (!TCPSocket::connect(host, port))
		{
			return false;
		}

		int res = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, nullptr, 0);
		res = mbedtls_ssl_config_defaults(&conf,
			MBEDTLS_SSL_IS_CLIENT,
			MBEDTLS_SSL_TRANSPORT_STREAM,
			MBEDTLS_SSL_PRESET_DEFAULT);

		if (res != 0)
		{
			set_errstr(res);
			close();
			return false;
		}

		mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);

		// this performs platform specific initialisation by retrieving the certificate store from the OS.
		// if implemented for this platform, it also sets mbedtls_ssl_conf_authmode to MBEDTLS_SSL_VERIFY_REQUIRED
		GameSparks::Util::CertificateStore::setup(conf);

		mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
		mbedtls_ssl_conf_dbg(&conf, debug_print, stderr);

		res = mbedtls_ssl_setup(&ssl, &conf);
		if (res != 0)
		{
			set_errstr(res);
			close();
			return false;
		}

		res = mbedtls_ssl_set_hostname(&ssl, host);
		if (res != 0)
		{
			set_errstr(res);
			close();
			return false;
		}

		mbedtls_ssl_set_bio(&ssl, &net, mbedtls_net_send, mbedtls_net_recv, 0);// , mbedtls_net_recv_timeout);

		do res = mbedtls_ssl_handshake(&ssl);
		while (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE);

		if (res != 0)
		{
			set_errstr(res);
			close();
			return false;
		}

		uint32_t flags;
		if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0)
		{
			char vrfy_buf[512];
			mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
			set_errstr(vrfy_buf);
			close();
			return false;
		}

		is_connected = true;
		return true;
	}

	virtual void close()
	{
		TCPSocket::close();
		is_connected = false;
	}

	virtual int send(const char *buf, size_t siz)
	{
		return mbedtls_ssl_write(&ssl, (unsigned char *)buf, siz);
		/*int ret = 0;
		do ret = mbedtls_ssl_write(&ssl, (unsigned char *)buf, siz);
		while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
		if (ret < 0) set_errstr(ret);
		return ret;*/
	}

	virtual int recv(char *buf, size_t siz)
	{
		return mbedtls_ssl_read(&ssl, (unsigned char *)buf, siz);
		/*int ret = 0;
		do ret = mbedtls_ssl_read(&ssl, (unsigned char *)buf, siz);
		while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
		if (ret < 0) set_errstr(ret);
		return ret;*/
	}
};

BaseSocket *BaseSocket::create(bool useSSL) {
	if (useSSL) {
		return new TLSSocket();
	}
	else {
		return new TCPSocket();
	}
}

#endif