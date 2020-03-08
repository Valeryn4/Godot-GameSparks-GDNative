#ifndef _GAMESPARKSRT_PACKET_HPP_
#define _GAMESPARKSRT_PACKET_HPP_

//#include <list>
//#include <vector>
//#include <memory>

#include "../../../include/GameSparks/gsstl.h"

#include "../../../include/System/Nullable.hpp"
#include "../../../include/System/Bytes.hpp"

#include "../../../include/GameSparksRT/Forwards.hpp"
#include "../../../include/GameSparksRT/RTData.hpp"

#include "../../System/Failable.hpp"

namespace System
{
	namespace IO
	{
		class Stream;
		class BinaryReader;
	}
}

namespace GameSparks { namespace RT { namespace Proto {

	class Packet
	{
		public:
			Packet() : Session(nullptr) {}
			Packet(IRTSessionInternal& Session_) : Session(&Session_) {}

			int OpCode = gsstl::numeric_limits<int>::lowest();
			System::Nullable<int> SequenceNumber;
			System::Nullable<int> RequestId;
			gsstl::list<int> TargetPlayers;
			System::Nullable<int> Sender;
			System::Nullable<bool> Reliable;
			RTData Data;
			System::Bytes Payload;

			// extensions
			IRTSessionInternal* Session;// = nullptr;
			const GameSparks::RT::Commands::RTRequest* Request = nullptr;
			gsstl::unique_ptr<IRTCommand> Command;
			bool hasPayload = false;
			System::Failable<void> WritePayload (System::IO::Stream& stream) const;

			//serializer
			static System::Failable<void> DeserializeLengthDelimited(System::IO::Stream& stream, System::IO::BinaryReader& br, Packet& instance);
			static System::Failable<void> Serialize(System::IO::Stream& stream, const Packet& instance);
			static System::Failable<int> SerializeLengthDelimited(System::IO::Stream &stream, const Packet &instance);

			//#if defined(_MSC_VER) && _MSC_VER <= 1900
			// visual studio 2013 does not support generating default move constructors and assignemnt operators
			// http://stackoverflow.com/questions/26581262/default-move-constructor-in-visual-studio-2013-update-3
			// Note: apparently other compilers also behave this way - so we enable this implementation by default
			Packet(Packet&& o)
			:OpCode(gsstl::move(o.OpCode))
			,SequenceNumber(gsstl::move(o.SequenceNumber))
			,RequestId(gsstl::move(o.RequestId))
			,TargetPlayers(gsstl::move(o.TargetPlayers))
			,Sender(gsstl::move(o.Sender))
			,Reliable(gsstl::move(o.Reliable))
			,Data(gsstl::move(o.Data))
			,Payload(gsstl::move(o.Payload))
			,Session(gsstl::move(o.Session))
			,Request(gsstl::move(o.Request))
			,Command(gsstl::move(o.Command))
			,hasPayload(gsstl::move(o.hasPayload))
			{}

			Packet& operator =(Packet&& o)
			{
				OpCode = gsstl::move(o.OpCode);
				SequenceNumber = gsstl::move(o.SequenceNumber);
				RequestId = gsstl::move(o.RequestId);
				TargetPlayers = gsstl::move(o.TargetPlayers);
				Sender = gsstl::move(o.Sender);
				Reliable = gsstl::move(o.Reliable);
				Data = gsstl::move(o.Data);
				Payload = gsstl::move(o.Payload);
				Session = gsstl::move(o.Session);
				Request = gsstl::move(o.Request);
				Command = gsstl::move(o.Command);
				hasPayload = gsstl::move(o.hasPayload);
				return *this;
			}
			//#endif
		private:
			// extensions
			System::Failable<void> ReadPayload (System::IO::Stream& stream);

			//#if defined(_MSC_VER) && _MSC_VER <= 1900
			Packet(const Packet&);
			Packet& operator =(const Packet&);
			//#endif
	};

}}} /* namespace GameSparks.RT.Proto */

#endif /* _GAMESPARKSRT_PACKET_HPP_ */
