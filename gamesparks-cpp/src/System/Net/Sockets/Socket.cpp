//#include <cassert>
//#include <iostream>
//#include <iomanip>

#include "./Socket.hpp"
#include "../../ObjectDisposedException.hpp"
#include "../../Threading/Thread.hpp"
#include "../../../../include/mbedtls/error.h"

#if (defined(__APPLE__) || defined(ANDROID) || defined(__linux__)) && !defined(IW_SDK)
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#   if defined(ANDROID)
#       include <fcntl.h>
#   else
#       include <sys/fcntl.h>
#   endif
//#include <iostream>
#endif

#if defined(WIN32)
#	include <Winsock2.h>
#endif

#if defined(IW_SDK)
//#   include <s3eDebug.h>
//#   include <IwGxPrint.h>
#   include <s3e.h>
//#   include <IwGx.h>
#endif /**/

#if defined(NN_NINTENDO_SDK)
// if you get a compile error here, please contact Game Sparks support with proof of you Nintendo Developer status for the support files.
#include "../../../mbedtls/mbed_tls_net_nintendo_switch_pre.inc"
#endif

#include <mutex>

namespace System { namespace Net { namespace Sockets {

static gsstl::string mbedtls_error_to_string(int result)
{
    assert(result < 0); // you cannot translate an error that is none.

	char buf[256];
	mbedtls_strerror(result, buf, sizeof(buf));
	return buf;
}


Socket::Socket(AddressFamily addressFamily, SocketType socketType, ProtocolType protocolType_)
:isTearingDown(false)
,isInsideInternalRecv(false)
,protocolType(protocolType_)
{
	(void)addressFamily;
	(void)socketType;
    assert(addressFamily == AddressFamily::InterNetwork);
    assert(
        (socketType == SocketType::Stream && protocolType == ProtocolType::Tcp) ||
        (socketType == SocketType::Dgram  && protocolType == ProtocolType::Udp)
    );

    mbedtls_net_init(&netCtx);

#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
    connectThread = NULL;
#endif
}

Socket::~Socket()
{
	teardown();

#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
    mbedtls_net_free(&netCtx);

    if (connectThread) {
        delete connectThread;
        connectThread = NULL;
    }
#endif
}

void Socket::Close()
{
    if(state == State::CONNECTED)
    {
        // wait for the recv to timeout
        while(isInsideInternalRecv)
        {
#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
            FPlatformProcess::Sleep(1.0f / 60.0f);
#else
            gsstl::this_thread::sleep_for(gsstl::chrono::milliseconds(1000/60));
#endif
        }
        mbedtls_net_free(&netCtx);

        state = State::CLOSED;
    }
}

#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
bool Socket::Init()
{
    return true;
}
 
uint32 Socket::Run()
{
    if(Connect(_endpoint))
    {
        if(!this->isTearingDown)
        {
            _requestCallback({});
        }
        else
        {
            gsstl::cerr << "INFO: connect finished on torn down socket." << gsstl::endl;
        }
    }

    return 0;
}

void Socket::Stop()
{
}
#endif

void Socket::teardown()
{
	if (isTearingDown) return;
    isTearingDown = true;

    // wait for the recv to timeout
    while(isInsideInternalRecv)
    {
#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
        FPlatformProcess::Sleep(1.0f / 60.0f);
#else
        gsstl::this_thread::sleep_for(gsstl::chrono::milliseconds(1000/60));
#endif
    }


#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
    if (connectThread) {
        while(state == State::CONNECTING)
        {
            FPlatformProcess::Sleep(1.0f / 60.0f);
        }

        connectThread->Kill(false);
    }
#else
    if(connectThread.joinable())
    {
#if defined(IW_SDK)
        // Unfortunately we need to wait for the connecting thread to exit, because marmalade does not cope well with the situation where
        // a socket is closed wile it's in use.
        
        while(state == State::CONNECTING)
        {
            gsstl::this_thread::sleep_for(gsstl::chrono::milliseconds(1000/60));
        }
#endif /* defined(IW_SDK) */
        
        mbedtls_net_free(&netCtx); // this closes the socket and aborts any potential blocking reads()

        connectThread.join();
    }

    mbedtls_net_free(&netCtx);
#endif
}

void Socket::SetSocketOption(SocketOptionLevel optionLevel, SocketOptionName optionName, bool optionValue)
{
    if(optionLevel == SocketOptionLevel::Socket && optionName == SocketOptionName::KeepAlive)
    {
        #if defined(SOL_SOCKET) || defined(NN_NINTENDO_SDK)
            int optval = optionValue?1:0;
            auto optlen = sizeof(optval);
            auto result = setsockopt(netCtx.fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, int(optlen));
			(void)result;
			assert(result >= 0);
		#elif defined(WIN32)
        
		#else
        int TODO_implement_socket_keepalive_for_this_platform;
        //#   warning "TODO: Socket Keepalive implementation for this platform"
        #endif
    }
    else
    {
        assert(false);
    }
}

void Socket::NoDelay(bool value)
{
    switch(state)
    {
        case State::CLOSED:
        case State::CONNECTING:
            scheduleNoDelayFlag = value;
            break;
        case State::CONNECTED:
            scheduleNoDelayFlag = value;
            DoNoDelay(value);
            break;
        default:
            assert(false);
            break;
    }
}


void Socket::DoNoDelay(bool value)
{
    #if defined(TCP_NODELAY) || defined(NN_NINTENDO_SDK)
        int flag = value?1:0;
        int result = setsockopt(netCtx.fd,            /* socket affected */
                                IPPROTO_TCP,     /* set option at TCP level */
                                TCP_NODELAY,     /* name of option */
                                (char*)&flag,
                                sizeof(flag));    /* length of option value */
		(void)result;
        assert(result >= 0);
    #else
    int TODO_implement_tcp_nodelay_for_this_platform;
    //#   warning "TODO: implement TCP_NODELAY for this platform"
    #endif
}

bool Socket::NoDelay() const
{
	assert(false); // this is not used ?
	return false;

	#if 0
		#if defined(TCP_NODELAY)
			assert(state == State::CONNECTED);
			int flag;
			int len = sizeof(flag);
			int result = getsockopt(netCtx.fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, &len);
			assert(result >= 0);
			assert(len == sizeof(flag));
			return flag == 1;
		#else
		#   warning "TODO: implement TCP_NODELAY for this platform"
		return false;
		#endif
	#endif
}

void Socket::BeginConnect(
        const IPEndPoint& endpoint,
        const AsyncCallback& requestCallback
)
{
    assert(state == State::CLOSED);
    state = State::CONNECTING;

#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
    _endpoint = endpoint;
    _requestCallback = requestCallback;

    if(protocolType == ProtocolType::Tcp) {
        connectThread = FRunnableThread::Create(this, TEXT("TCP Connect thread") , 0, TPri_BelowNormal);
    } else {
        connectThread = FRunnableThread::Create(this, TEXT("UDP Connect thread") , 0, TPri_BelowNormal);
    }
#else
    connectThread = gsstl::thread([&, endpoint, requestCallback](){
        if(protocolType == ProtocolType::Tcp)
            System::Threading::Thread::SetName("TCP Connect thread");
        else
            System::Threading::Thread::SetName("UDP Connect thread");

        if(Connect(endpoint))
        {
            if(!this->isTearingDown)
            {
                requestCallback({});
            }
            else
            {
                gsstl::cerr << "INFO: connect finished on torn down socket." << gsstl::endl;
            }
        }
        //assert(protocolType == ProtocolType::Tcp); // the udp thread should never stop
    });
#endif
}


bool Socket::Connect(const IPEndPoint &endpoint) {
    state = State::CONNECTING;
    int result = mbedtls_net_connect(&netCtx, endpoint.Host.c_str(), endpoint.Port.c_str(), protocolType==ProtocolType::Tcp?MBEDTLS_NET_PROTO_TCP:MBEDTLS_NET_PROTO_UDP);

    if(result == 0 && netCtx.fd != -1)
    {
        state = State::CONNECTED;
        if(this->protocolType == ProtocolType::Tcp)
        {
            DoNoDelay(scheduleNoDelayFlag);
        }
        return true;
    }
    else
    {
        state = State::CLOSED;

        if(!isTearingDown)
            gsstl::clog << "ERROR: Connect failed: " << mbedtls_error_to_string(result) << gsstl::endl;
        return false;
    }
}


struct AsyncReceiveResult : public IAsyncResult
{
    int numBytesRead = 0;
};


void Socket::BeginReceive(System::Bytes &buffer, const AsyncCallback &callback) {
    assert(buffer.size() > 0);
    if(protocolType == ProtocolType::Udp)
    {
        // BeginReceive of the UDP socket is called from the BeginconnectHandler and that callback is called
        // from the connect thread. So we're doing a blocking call here to be able to reuse that thread.
        AsyncReceiveResult ar;
        ar.numBytesRead = internalRecv(buffer.data(), buffer.size());

        if (!isTearingDown && ar.numBytesRead > 0)
        {
            callback(ar);
        }
        else
        {
            gsstl::cerr << "INFO: UDP-Socket read error: " << mbedtls_error_to_string(ar.numBytesRead) << gsstl::endl;
        }
    }
    else
    {
        assert(!this->receiveCallback); // you can't call BeginReceive twice before the old has completed
        assert(!this->receiveBuffer);
        this->receiveCallback = callback;
        this->receiveBuffer = &buffer;

        // set socket to non-blocking. dispatching is done in Poll()
        mbedtls_net_set_nonblock(&netCtx);
    }
}


int Socket::EndReceive(const System::IAsyncResult &res) {
	// meh - good old c-style-cast, because the compiler is complaining otherwise it at least one configuration.
	return ((const AsyncReceiveResult*)(&res))->numBytesRead;
	//return reinterpret_cast<const AsyncReceiveResult*>(&res)->numBytesRead;
    //Note: Unreal does not support RTTI, so we're just doing the cast.
    /*if(const AsyncReceiveResult* ar = dynamic_cast<const AsyncReceiveResult*>(&res))
    {
        return ar->numBytesRead;
    }
    else
    {
        assert(false);
        return -1;
    }*/
}


/// this method recv()s with a timeout so that socket teardowns can be gracefully handled.
int Socket::internalRecv(unsigned char *buf, size_t len)
{
    int result = -12345;
    isInsideInternalRecv = true;
    while(!isTearingDown && result == -12345)
    {
        result = mbedtls_net_recv_timeout(&netCtx, buf, len, 100);
    }
    isInsideInternalRecv = false;
    return result;
}


Failable<int> Socket::Receive(System::Bytes &buffer, int offset, int count) {
    if(isTearingDown)
        GS_THROW(System::ObjectDisposedException("Socket has closed or read error!"));

    assert(Connected());
    assert(static_cast<int>(buffer.size()) >= offset+count);

    auto set_blocking_result = mbedtls_net_set_block(&netCtx);
	(void)set_blocking_result;
    assert(set_blocking_result >= 0);

    assert(!this->receiveCallback); // you can't do a blocking read, while you have a non blocking read scheduled
    assert(!this->receiveBuffer);

    auto result = mbedtls_net_recv(&netCtx, buffer.data() + offset, count);
    if (result < 0)
    {
        GS_THROW(System::ObjectDisposedException("Socket has closed or read error:" + gsstl::string(mbedtls_error_to_string(result))));
    }
    if (result == 0)
    {
        GS_THROW(System::ObjectDisposedException("Socket has closed or read error: Zero bytes"));
    }
    return result;
}


Failable<int> Socket::Receive(System::Bytes &buffer) {
    return Receive(buffer, 0, (int)buffer.size());
}


bool Socket::Connected() const {
    return state == State::CONNECTED;
}


gsstl::mutex output_mutex;

gsstl::ostream& operator << (gsstl::ostream& os, const System::Bytes& bytes)
{
    (void)bytes;
/*
    std::lock_guard<std::mutex> guard(output_mutex);

    os << std::dec;
    std::copy(
        bytes.begin(),
        bytes.end(),
        std::ostream_iterator<char>(os, " ")
    );

    os << std::endl << std::hex << std::setfill('0') << std::setw(2);
    std::copy(
            bytes.begin(),
            bytes.end(),
            std::ostream_iterator<int>(os, "")
    );
*/
    return os;
}


Failable<void> Socket::Send(const System::Bytes &buffer, int offset, int size) {
    assert(static_cast<int>(buffer.size()) >= offset+size);
    //std::clog << "SEND:" << buffer << std::endl;
    int result = mbedtls_net_send(&netCtx, buffer.data()+offset, size);
    if (result < 0)
    {
        GS_THROW(ObjectDisposedException("Socket has closed or read error:" + gsstl::string(mbedtls_error_to_string(result))));
    }
    return {};
}

void Socket::Poll() {
    assert(protocolType == ProtocolType::Tcp);
    if(receiveCallback)
    {
        assert(receiveBuffer);
        AsyncReceiveResult ar;

		//ar.numBytesRead = mbedtls_net_recv(&netCtx, receiveBuffer->data(), receiveBuffer->size());

		Failable<int> res = Receive(*receiveBuffer);
		if (!res.isOK())
		{
			gsstl::clog << "Error: read() failed: " << res.GetException() << gsstl::endl;
			return;
		}
		ar.numBytesRead = res.GetResult();
		
		if(ar.numBytesRead == MBEDTLS_ERR_SSL_WANT_READ)
        {
            return; // nothing to do
        }
        else if(ar.numBytesRead < 0)
        {
            gsstl::clog << "ERROR: read() failed: " << mbedtls_error_to_string(ar.numBytesRead) << gsstl::endl;
            assert(false);
        }
        else
        {
            //std::clog << "RECV:" << receiveBuffer << std::endl;

            assert(receiveBuffer);
            assert(receiveCallback);
            auto callback = receiveCallback;
            receiveCallback = AsyncCallback();
            receiveBuffer = nullptr;
            callback(ar);
        }
    }
}

}}}
