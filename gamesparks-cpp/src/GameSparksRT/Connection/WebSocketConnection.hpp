#ifndef _GAMESPARKSRT_WEBSOCKETCONNECTION_HPP_
#define _GAMESPARKSRT_WEBSOCKETCONNECTION_HPP_

#if defined(_DURANGO)

#include "./Connection.hpp"
#include "../../System/IAsyncResult.hpp"
#include "../../System/Net/Sockets/TcpClient.hpp"
#include "../Proto/Packet.hpp"
#include "../../../include/easywsclient/easywsclient.hpp"

namespace GameSparks { namespace RT { namespace Connection {

	class WebSocketConnection : public Connection
	{
		public:
			WebSocketConnection(const gsstl::string& remotehost, const gsstl::string& remoteport, IRTSessionInternal* session);
			
			virtual System::Failable<int> Send(const Commands::RTRequest& request) override;
			virtual void StopInternal() override;

			void Poll();
		private:
			void ConnectCallback();
			static void DataReceived(const gsstl::string & message, void* This);
			static void ErrorCallback(const easywsclient::WSError& error, void* This);
			System::Failable<bool> read(PositionStream& stream, Proto::Packet& p);

			gsstl::unique_ptr<easywsclient::WebSocket> client;
			easywsclient::WebSocket::readyStateValues lastReadyState;
	};

}}} /* namespace GameSparks.RT.Connection */

#endif /* defined(_DURANGO) */

#endif /* _GAMESPARKSRT_WEBSOCKETCONNECTION_HPP_ */
