#ifndef _SYSTEM_NET_SOCKETS_SOCKET_HPP_
#define _SYSTEM_NET_SOCKETS_SOCKET_HPP_

#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
    #include <Runtime/Core/Public/HAL/Runnable.h>
#endif

#include "../../AsyncCallback.hpp"
#include "./AddressFamily.hpp"
#include "./SocketType.hpp"
#include "./ProtocolType.hpp"
#include "SocketOptionLevel.hpp"
#include "SocketOptionName.hpp"
#include "../../../../include/mbedtls/net.h"
#include "../IPEndPoint.hpp"
#include "../../Failable.hpp"
#include "../../../../include/System/Bytes.hpp"

namespace System {class IAsyncResult;}

namespace System { namespace Net { namespace Sockets {

#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
    class Socket : public FRunnable
#else
    class Socket
#endif    
    {
        public:
            Socket(AddressFamily addressFamily, SocketType socketType, ProtocolType protocolType);
            virtual ~Socket();

            void NoDelay(bool value);
            bool NoDelay() const;

            void SetSocketOption(SocketOptionLevel optionLevel, SocketOptionName optionName, bool optionValue);

            void BeginConnect(
                    const IPEndPoint& endpoint,
                    const AsyncCallback& requestCallback
            );

            void BeginReceive(
                    System::Bytes& buffer,
                    const AsyncCallback& callback
            );
            // https://msdn.microsoft.com/en-us/library/w7wtt64b(v=vs.110).aspx
            int EndReceive(const System::IAsyncResult& res);

            Failable<int> Receive(System::Bytes &buffer);
            virtual Failable<int> Receive(System::Bytes &buffer, int offset, int count);

            bool Connected() const;

            virtual Failable<void> Send(const System::Bytes &buffer, int offset, int size);

            void Poll();

			void Close();

#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
            virtual bool Init();
            virtual uint32 Run();
            virtual void Stop();
#endif

        protected:
            virtual int internalRecv(unsigned char *buf, size_t len);
			virtual bool Connect(const IPEndPoint& endpoint);

            enum class State
            {
                CLOSED,
                CONNECTING,
                CONNECTED,
            };
            State state = State::CLOSED;
			mbedtls_net_context netCtx;
			volatile bool isTearingDown;
			AsyncCallback receiveCallback;
			System::Bytes* receiveBuffer = nullptr;

#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
            IPEndPoint _endpoint;
            AsyncCallback _requestCallback;
            FRunnableThread *connectThread;
#else
			gsstl::thread connectThread; // for name resolution and connect()
#endif	
    		volatile bool isInsideInternalRecv;
			void teardown();
		private:
			void DoNoDelay(bool value);
			bool scheduleNoDelayFlag = false;
            ProtocolType protocolType;
    };

}}}

#endif /* _SYSTEM_NET_SOCKETS_SOCKET_HPP_ */
