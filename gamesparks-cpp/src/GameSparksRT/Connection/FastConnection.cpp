//#include <thread>
//#include <iostream>
#include "../Commands/Requests/LoginCommand.hpp"
#include "../../System/Exception.hpp"
#include "./FastConnection.hpp"
#include "../../System/Threading/Thread.hpp"
#include "../../GameSparks/GSClientConfig.h"

namespace System {class IAsyncResult;}

namespace GameSparks { namespace RT { namespace Connection {

FastConnection::FastConnection(const gsstl::string &remotehost, const gsstl::string& port,
                               IRTSessionInternal *session, gsstl::recursive_mutex& sessionSendMutex)
    : Connection(remotehost, port, session)
{
    callback = [this](const System::IAsyncResult& ar){Recv(ar);};
    client.EnableBroadcast(false);
    client.ExclusiveAddressUse(false);
    client.MulticastLoopback(false);
    client.BeginConnect (remoteEndPoint, [this, &sessionSendMutex](const System::IAsyncResult& /*ar*/){

        // we need to lock sessionSendMutex first to avoid ABA deadlock
        {
            gsstl::lock_guard<gsstl::recursive_mutex> lock1(sessionSendMutex);
            gsstl::lock_guard<gsstl::recursive_mutex> lock2(sessionMutex);
            if (!this->session) return;
            DoLogin ();
        }
        client.Client().BeginReceive (buffer, callback);
    });
    //client.Connect (remoteEndPoint);
    //session->Log("FastConnection", GameSparksRT::LogLevel::DEBUG, "UDP Address=" + client.Client().LocalEndPoint);
    //client.Client().BeginReceive (buffer, 0, GameSparksRT::MAX_MESSAGE_SIZE_BYTES, 0, callback);
    //client.Client().BeginReceive (buffer, callback);
    //DoLogin ();
}

System::Failable<int> FastConnection::Send(const Commands::RTRequest &request) {
    System::IO::MemoryStream ms_;

    GS_CALL_OR_THROW(ms_.Position(0));
    Proto::Packet p = request.ToPacket(*session, true);

    GS_TRY
    {
        GS_CALL_OR_CATCH(Proto::Packet::SerializeLengthDelimited(ms_, p));
    }
    GS_CATCH(e) {(void)e;}

    GS_CALL_OR_THROW(client.Send (ms_.GetBuffer(), ms_.Position()));

    return ms_.Position();
}

void FastConnection::StopInternal() {
    // TODO: check if we need to close
    //if(client != nullptr)
    //    client.Close ();
    gsstl::lock_guard<gsstl::recursive_mutex> lock(sessionMutex);
    session = nullptr;
}

void FastConnection::DoLogin() {
    int attempts = 1;

    GS_TRY
    {
        gsstl::lock_guard<gsstl::recursive_mutex> lock(sessionMutex);

        while (session != nullptr && session->GetConnectState() < GameSparksRT::ConnectState::ReliableAndFastSend)
        {
            Com::Gamesparks::Realtime::Proto::LoginCommand loginCmd(session->ConnectToken());
            GS_CALL_OR_CATCH(Send (loginCmd));

            // we iteratively sleep for short amounts so that we can check the exit condition more frequently.
            // this is to avoid excessive delays before calling session->OnReady()
            auto mustConnectIn = GameSparks::Core::GSClientConfig::instance().ComputeSleepPeriod(attempts);
            for(
                auto start = gsstl::chrono::steady_clock::now();
                gsstl::chrono::steady_clock::now() - start < gsstl::chrono::duration<float>(mustConnectIn) && // mustConnectIn expired?
                session != nullptr && session->GetConnectState() < GameSparksRT::ConnectState::ReliableAndFastSend // same condition as in the while above
                ;
            )
            {
                gsstl::this_thread::sleep_for(gsstl::chrono::milliseconds(100));
            }

            attempts++;
        }

        if(session)
        {
            session->OnReady (true);
        }
    }
    GS_CATCH(e) {(void)e;}
}

void FastConnection::Recv(const System::IAsyncResult& res)
{
    GS_TRY
    {
        int read = client.Client().EndReceive(res);
        ReadBuffer(read);
        GS_CALL_OR_CATCH(SyncReceive ());
    }
    GS_CATCH(e)
    {
        gsstl::lock_guard<gsstl::recursive_mutex> lock(sessionMutex);
        if(session)
        {
            session->Log("FastConnection EndReceive", GameSparksRT::LogLevel::LL_INFO, e.Format());
        }
        else
        {
            gsstl::clog << "FastConnection EndReceive:" << e.Format() << gsstl::endl;
        }
    }
    //finally
    {
        if (!stopped && session != nullptr)
        client.Client().BeginReceive (buffer, callback);
    }
}

#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wshadow"
#endif

#if defined(_MSC_VER)
#	pragma warning (push)
#	pragma warning (disable:4456)
#endif

void FastConnection::ReadBuffer(int read)
{
    GS_TRY
    {
		gsstl::lock_guard<gsstl::recursive_mutex> lg(sessionMutex);
		if (!session)
			return;

        BinaryWriteMemoryStream ms;
		GS_CALL_OR_CATCH(ms.Position(0));
        GS_CALL_OR_CATCH(ms.Write (buffer, 0, read));
        GS_CALL_OR_CATCH(ms.Position(0));

        while (ms.Position() < read) {
            GS_TRY
            {
                assert(session);
                Commands::Packet p(*session);
                GS_CALL_OR_CATCH(Commands::Packet::DeserializeLengthDelimited (ms, ms.BinaryReader, p));
                p.Reliable = p.Reliable.GetValueOrDefault (false);
                GS_CALL_OR_CATCH(OnPacketReceived (p));
                p = Commands::Packet(*session); // reset packet to default state
            }
            GS_CATCH(e)
            {
                gsstl::clog << e << gsstl::endl;
                static gsstl::mutex globalMutex;
                gsstl::lock_guard<gsstl::mutex> globalLock(globalMutex);
                gsstl::clog << "packet was:" << gsstl::endl;
                for(int i=0; i!=read; ++i)
                {
                    if(size_t(i) >= buffer.size())
                    {
                        gsstl::clog << gsstl::endl << "aborting buffer dump: i >= buffer.size(), i.e. i = " << i << ", buffer.size() = " << buffer.size();
                        break;
                    }
                    //TODO
                    //gsstl::clog << gsstl::hex << gsstl::setw(2) << gsstl::setfill('0') << int(buffer[i]);
                    gsstl::clog << gsstl::hex << int(buffer[i]);
                }
                gsstl::clog << gsstl::endl;
                return;
            }
        }
    }
    GS_CATCH(e)
    {
        session->Log ("FastConnection read", GameSparksRT::LogLevel::LL_WARN, e.Format());
    }
}

#if defined(_MSC_VER)
#	pragma warning (pop)
#endif

#if defined(__clang__)
#   pragma clang diagnostic pop
#endif /* __clang__ */

System::Failable<void> FastConnection::SyncReceive() {
    while (!stopped && session != nullptr) {
        GS_ASSIGN_OR_THROW(read, client.Client().Receive (buffer));
        if (read > 0) {
            ReadBuffer (read);
        }
    }
    return {};
}

}}} /* namespace GameSparks.RT.Connection */
