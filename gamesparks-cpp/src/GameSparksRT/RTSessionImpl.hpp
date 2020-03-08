#ifndef _GAMESPARKSRT_RTSESSIONIMPL_HPP_
#define _GAMESPARKSRT_RTSESSIONIMPL_HPP_

/*#include <map>
#include <queue>
#include <thread>
#include <mutex>*/

#include "../../include/GameSparksRT/Forwards.hpp"
#include "./IRTSessionInternal.hpp"
#include "./IRTCommand.hpp"

#if defined(_DURANGO)
#	define GS_RT_OVER_WS   1
#else
#	define GS_RT_OVER_WS   0
#endif

namespace GameSparks { namespace RT {

	class RTSessionImpl : public IRTSessionInternal
	{
		public:
			RTSessionImpl (const gsstl::string& connectToken, const gsstl::string& hostName, const gsstl::string& tcpPort, const gsstl::string& udpPort);
			 virtual ~RTSessionImpl();

			virtual int SendData(int opCode, GameSparksRT::DeliveryIntent intent, const System::Bytes &payload,
								 const RTData &data, const gsstl::vector<int> &targetPlayers) override;

            virtual int SendRTData(int opCode, GameSparksRT::DeliveryIntent deliveryIntent, const RTData &data,
								   const gsstl::vector<int> &targetPlayers) override;

            virtual int SendBytes(int opCode, GameSparksRT::DeliveryIntent deliveryIntent,
								  const System::ArraySegment<System::Byte> &payload,
								  const gsstl::vector<int> &targetPlayers) override;

            virtual int SendRTDataAndBytes(int opCode, GameSparksRT::DeliveryIntent intent,
										   const System::ArraySegment<System::Byte> &payload, const RTData &data,
										   const gsstl::vector<int> &targetPlayers) override;

    		virtual void Stop() override;
			virtual void Start() override;
			virtual void Update() override;
			virtual gsstl::string ConnectToken() const override;
			virtual void ConnectToken(const gsstl::string& token) override;
			virtual gsstl::string FastPort() const override;
			virtual void FastPort(const gsstl::string&) override;

			virtual void ConnectReliable() override;
			virtual void ConnectFast() override;
			virtual bool ShouldExecute(int peerId, System::Nullable<int> sequence) override;
			virtual void SubmitAction(gsstl::unique_ptr<IRTCommand>& action) override;
			virtual int NextSequenceNumber() override;
			virtual void OnPlayerConnect(int peerId) override;
			virtual void OnPlayerDisconnect(int peerId) override;
			virtual void OnReady(bool ready) override;
			virtual void OnPacket(const RTPacket &packet) override;


			virtual GameSparksRT::ConnectState GetConnectState() const override;
			virtual void SetConnectState(GameSparksRT::ConnectState value) override;

	private:
			virtual void DoLog(const gsstl::string &tag, GameSparks::RT::GameSparksRT::LogLevel level, const gsstl::string &msg) override;
			void ResetSequenceForPeer (int peerId);
			void CheckConnection();
			gsstl::unique_ptr<IRTCommand> GetNextAction();

			// note: it's important, that those two are the first members so that they are created first and destroyed last.
			gsstl::queue<gsstl::unique_ptr<IRTCommand>> actionQueue;
			gsstl::mutex actionQueueMutex;

			#if GS_RT_OVER_WS
			gsstl::unique_ptr<Connection::WebSocketConnection> reliableConnection;
			#else
			gsstl::unique_ptr<Connection::ReliableConnection> reliableConnection;
			gsstl::unique_ptr<Connection::FastConnection> fastConnection;
			#endif

			bool running = false;
			gsstl::chrono::steady_clock::time_point mustConnnectBy;
			int connectionAttempts;

			gsstl::string connectToken;
			gsstl::string hostName;
			gsstl::string TcpPort;
			gsstl::string fastPort;
			gsstl::map<int, int> peerMaxSequenceNumbers;

			int sequenceNumber = 0;

			GameSparksRT::ConnectState internalState = GameSparksRT::ConnectState::Disconnected;

			gsstl::recursive_mutex sendMutex;
	};

}} /* namespace GameSparks.RT */

#endif /* _GAMESPARKSRT_RTSESSIONIMPL_HPP_ */
