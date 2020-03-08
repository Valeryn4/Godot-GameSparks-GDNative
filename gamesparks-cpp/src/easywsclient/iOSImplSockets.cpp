#include "GameSparks/GSPlatformDeduction.h"
#include "easywsclient/easywsclient.hpp"

#if (GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC)

#include <CoreFoundation/CoreFoundation.h>
#include <CFNetwork/CFNetwork.h>
#include <CFNetwork/CFHTTPStream.h>
#include "GameSparks/GSLeakDetector.h"
#include "GameSparks/GSUtil.h"

class BaseSocket;

void readCallback(CFReadStreamRef stream, CFStreamEventType event, void* ptr)
{
    (void)stream;
    (void)event;
    (void)ptr;
}

class TCPSocket : public BaseSocket
{
private:
	bool is_connected;
	volatile bool is_aborted;

protected:
	bool is_blocking;
    bool is_secured;
    CFReadStreamRef readStream;
    CFWriteStreamRef writeStream;

public:
	TCPSocket() : is_connected(false), is_aborted(false), is_blocking(true), is_secured(false)
	{
        readStream = NULL;
        writeStream = NULL;
	}

	virtual ~TCPSocket()
	{
		close();
	}

	virtual bool connect(const char *host, short port)
	{
        CFStringRef str;
       
		if (is_connected) {
			return true;
		}
        
        str = CFStringCreateWithCStringNoCopy(NULL, host, kCFStringEncodingMacRoman, NULL);

        CFStreamCreatePairWithSocketToHost(NULL, str, port, &readStream, &writeStream);
	
        CFRelease(str);
        
        if (!readStream || !writeStream) {
            fprintf(stderr, "Failed to create pair with socket to host!\n");
            
            close();
            
            return false;
        }
        
        if (is_secured) {
            if (!CFReadStreamSetProperty(readStream, kCFStreamPropertySocketSecurityLevel, kCFStreamSocketSecurityLevelNegotiatedSSL)) {
                fprintf(stderr, "Failed to set up a secure CFReadStream!\n");
                
                close();
                
                return false;
            }
            
            if (!CFWriteStreamSetProperty(writeStream, kCFStreamPropertySocketSecurityLevel, kCFStreamSocketSecurityLevelNegotiatedSSL)) {
                fprintf(stderr, "Failed to set up a secure CFWriteStream!\n");
                
                close();
                
                return false;
            }
        }
        
        CFRetain(readStream);
        CFRetain(writeStream);
        
        if (!CFReadStreamOpen(readStream)) {
            fprintf(stderr, "Failed to connect TCP socket (CFReadStream not opened)!\n");
            
            close();
            
            return false;
        }
        
        if (!CFWriteStreamOpen(writeStream)) {
            fprintf(stderr, "Failed to connect TCP socket (CFWriteStream not opened)!\n");
            
            close();
            
            return false;
        }
        
		if (is_aborted) {
            printf("Connection aborted!\n");
			
			return false;
		}

		is_connected = true;

		return true;
	}

	virtual void close()
	{
        is_connected = false;

        if (readStream) {
            CFReadStreamClose(readStream);
            CFRelease(readStream);
            readStream = NULL;
        }
        if (writeStream) {
            CFWriteStreamClose(writeStream);
            CFRelease(writeStream);
            writeStream = NULL;
        }
	}

	virtual int send(const char *buf, size_t siz)
	{
		if (is_aborted || !writeStream) {
			return -1;
		}

        if (is_blocking || CFWriteStreamCanAcceptBytes(writeStream)) {
            return CFWriteStreamWrite(writeStream, (const UInt8 *)buf, (int)siz);
        } else {
            return -1;
        }
	}

	virtual int recv(char *buf, size_t siz)
	{
		if (is_aborted || !readStream) {
			return -1;
		}

        if (is_blocking || CFReadStreamHasBytesAvailable(readStream)) {
            return CFReadStreamRead(readStream, (UInt8 *)buf, (int)siz);
        } else {
            return -1;
        }
	}

	virtual void set_blocking(bool should_block)
	{
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
    bool is_connected;

	static void debug_print(void *ctx, int level, const char *file, int line, const char *str)
	{
		((void)level);
		fprintf((FILE *)ctx, "%s:%04d: %s", file, line, str);
		//fflush((FILE *)ctx);
	}

public:
	TLSSocket() : is_connected(false)
	{
        is_secured = true;
	}

	virtual ~TLSSocket()
	{
	}

	virtual bool connect(const char *host, short port)
	{
		if (is_connected) {
			return true;
		}

		if (!TCPSocket::connect(host, port)) {
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
        return TCPSocket::send(buf, siz);
	}

	virtual int recv(char *buf, size_t siz)
	{
        return TCPSocket::recv(buf, siz);
	}

	virtual void set_blocking(bool should_block)
	{
		TCPSocket::set_blocking(should_block);
	}
};

BaseSocket *BaseSocket::create(bool useSSL) {
	if (useSSL) {
		return new TLSSocket();
	} else {
		return new TCPSocket();
	}
}

#endif
