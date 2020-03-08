#if defined(_DURANGO)

#include "../../System/Exception.hpp"
//#include <iostream>
#include "./WebSocketConnection.hpp"
#include "../Commands/Requests/LoginCommand.hpp"
#include "../Proto/PositionStream.hpp"


namespace GameSparks { namespace RT { namespace Connection {

using namespace System::Net::Sockets;
using namespace System;
using namespace GameSparks::RT::Commands;
using namespace Com::Gamesparks::Realtime::Proto;

static gsstl::string build_url(const gsstl::string& remotehost, const gsstl::string& remoteport)
{
	return "wss://" + remotehost + ":" + remoteport;
}

WebSocketConnection::WebSocketConnection(const gsstl::string& remotehost, const gsstl::string& remoteport, IRTSessionInternal* session)
:Connection(remotehost, remoteport, session)
,client(easywsclient::WebSocket::from_url_binary(build_url(remotehost, remoteport)))
,lastReadyState(easywsclient::WebSocket::CLOSED)
{
}


System::Failable<int> WebSocketConnection::Send(const Commands::RTRequest& request)
{
	assert(client);

	if (client && client->getReadyState() == easywsclient::WebSocket::OPEN)
	{
		Packet p = request.ToPacket(*session, false);
		GS_TRY
		{
			System::IO::MemoryStream clientStream;
			GS_ASSIGN_OR_CATCH(ret, Packet::SerializeLengthDelimited(clientStream, p));
			auto msg = System::Text::Encoding::UTF8::GetString(clientStream.GetBuffer());
			msg.resize(clientStream.Position());
			client->send(msg);
			return ret;
		}
		GS_CATCH(e)
		{
			gsstl::clog << e << gsstl::endl;
			if (session != nullptr && !stopped) {
				session->SetConnectState(GameSparksRT::ConnectState::Disconnected);
				session->Log("ReliableConnection", GameSparksRT::LogLevel::LL_DEBUG, e.Format());

				// without exception support enabled in the compiler, we're not able to catch exceptions in client code
				GS_TRY
				{
					session->OnReady(false);
				} GS_CATCH(e) { (void)e; }
			}
			return 0;
		}
	}
	return -1;
}


void WebSocketConnection::StopInternal()
{
	if (client && client->getReadyState() != easywsclient::WebSocket::CLOSED && client->getReadyState() != easywsclient::WebSocket::CLOSING)
	{
		client->close();
	}
	session = nullptr;
}


void WebSocketConnection::Poll()
{
	assert(client);

	if (client)
	{
		if (client->getReadyState() != easywsclient::WebSocket::CLOSED)
		{
			client->poll(0, WebSocketConnection::ErrorCallback, this);
			client->dispatch(WebSocketConnection::DataReceived, WebSocketConnection::ErrorCallback, this);
		}

		if (lastReadyState != easywsclient::WebSocket::OPEN && client->getReadyState() == easywsclient::WebSocket::OPEN)
		{
			this->ConnectCallback();
		}

		lastReadyState = client->getReadyState();
	}
}


void WebSocketConnection::ConnectCallback()
{
	//Each time a tcp connection is established we re-authenticate
	GS_TRY
	{
		LoginCommand loginCmd(session->ConnectToken());
		GS_CALL_OR_CATCH(Send(loginCmd));
	}
	GS_CATCH(e)
	{
		gsstl::lock_guard<gsstl::recursive_mutex> lock(sessionMutex);
		if (session != nullptr && !stopped) {
			session->SetConnectState(GameSparksRT::ConnectState::Disconnected);

			session->Log("ReliableConnection", GameSparksRT::LogLevel::LL_DEBUG, e.Format());

			//session->Log ("ReliableConnection", GameSparksRT::LogLevel::DEBUG, e.StackTrace);
			session->OnReady(false);
		}
	}
}


void WebSocketConnection::DataReceived(const gsstl::string & message, void* This)
{
	assert(This);
	WebSocketConnection* self = (WebSocketConnection*)This;
	if (self)
	{
		Packet p(*self->session);
		System::IO::MemoryStream ms;
		ms.Write(System::Text::Encoding::UTF8::GetBytes(message), 0, int(message.size()));
		ms.Seek(0, System::IO::SeekOrigin::Begin);
		PositionStream rss(ms);

		GS_TRY
		{
			GS_ASSIGN_OR_CATCH(success, self->read(rss, p));
			if (success)
			{
				self->OnPacketReceived(p);
				gsstl::clog << "DataReceived  " << message.size();
			}
			else
			{
				GS_PASS_EXCEPTION_TO_CATCH(System::Exception("read failed"));
			}
		}
		GS_CATCH(e)
		{
			gsstl::lock_guard<gsstl::recursive_mutex> lock(self->sessionMutex);
			if (self->session != nullptr) {
				self->session->SetConnectState(GameSparksRT::ConnectState::Disconnected);

				self->session->Log("ReliableConnection", GameSparksRT::LogLevel::LL_DEBUG, e.Format());

				//session->Log ("ReliableConnection", GameSparksRT::LogLevel::DEBUG, e.StackTrace);
				self->session->OnReady(false);
			}
		}
	}
}


System::Failable<bool> WebSocketConnection::read(PositionStream& stream, Packet& p)
{
	assert(client);
	if (!client || client->getReadyState() != easywsclient::WebSocket::OPEN) {
		return false;
	}

	GS_CALL_OR_THROW(Packet::DeserializeLengthDelimited(stream, stream.BinaryReader, p));
	//p.Session = session;
	p.Reliable = p.Reliable.GetValueOrDefault(true);
	return true;
}


void WebSocketConnection::ErrorCallback(const easywsclient::WSError& error, void* This)
{
	assert(This);
	WebSocketConnection* self = (WebSocketConnection*)This;
	if (self && self->session)
	{
		self->session->Log("WebSocketConnection", GameSparksRT::LogLevel::LL_DEBUG, "ErrorCallback :" + error.message);
	}
}


}}} /* namespace GameSparks.RT.Connection */

#endif /* defined(_DURANGO) */
