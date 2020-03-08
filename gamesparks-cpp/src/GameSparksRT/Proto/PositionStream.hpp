#ifndef _GAMESPARKSRT_POSITIONSTREAM_HPP_
#define _GAMESPARKSRT_POSITIONSTREAM_HPP_

#include "../../System/IO/Stream.hpp"
#include "../../System/IO/BinaryReader.hpp"
#include <vector>
#include "../../../include/System/Bytes.hpp"
#include "../../System/Failable.hpp"

namespace GameSparks { namespace RT {

	class PositionStream : public System::IO::Stream
	{
		public:
			PositionStream(System::IO::Stream& baseStream)
			:BytesRead(0)
			, BinaryReader(*this)
			, stream(baseStream)
			{}

			int BytesRead = 0;

			System::IO::BinaryReader BinaryReader;

			//virtual void WriteByte(unsigned char Value) override;

			virtual System::Failable<int> Read(System::Bytes& buffer, int offset, int count) override;
			virtual System::Failable<int> ReadByte() override;

			virtual int Position() const override;
			virtual System::Failable<void> Position(
				const int /*pos*/) override {
				GS_PROGRAMMING_ERROR("not implemented"); GS_THROW(System::NotImplementedException("NotImplementedException"));
			}

			virtual bool CanRead() const override;

	private:
			System::IO::Stream& stream;
	};

}} /* namespace GameSparks.RT */

#endif /* _GAMESPARKSRT_POSITIONSTREAM_HPP_ */
