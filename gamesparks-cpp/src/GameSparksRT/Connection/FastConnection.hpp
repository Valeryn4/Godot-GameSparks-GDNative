#ifndef _GAMESPARKSRT_FASTCONNECTION_HPP_
#define _GAMESPARKSRT_FASTCONNECTION_HPP_

#include "./Connection.hpp"
#include "../../../include/GameSparksRT/Forwards.hpp"
#include "../../System/Net/Sockets/UdpClient.hpp"
#include "../Proto/ReusableBinaryWriter.hpp"
#include "../../System/AsyncCallback.hpp"

namespace System {class IAsyncResult;}

namespace GameSparks { namespace RT { namespace Connection {

	class FastConnection : public Connection
	{
		public:
			FastConnection (const gsstl::string& remotehost, const gsstl::string& port, IRTSessionInternal* session, gsstl::recursive_mutex& sessionSendMutex);
			virtual System::Failable<int> Send(const Commands::RTRequest &request) override;
			virtual void StopInternal() override;

			System::Bytes buffer = System::Bytes(GameSparksRT::MAX_MESSAGE_SIZE_BYTES);
		private:
			void DoLogin();
			void Recv(const System::IAsyncResult& res);
			void ReadBuffer(int read);
			System::Failable<void> SyncReceive();

			System::Net::Sockets::UdpClient client;

			System::AsyncCallback callback;
	};

}}} /* namespace GameSparks.RT.Connection */

#endif /* _GAMESPARKSRT_FASTCONNECTION_HPP_ */
