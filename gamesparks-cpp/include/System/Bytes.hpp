#ifndef _SYSTEM_BYTES_HPP_INCLUDED_
#define _SYSTEM_BYTES_HPP_INCLUDED_

#include <vector>
#include <GameSparks/gsstl.h>

/// commonly used data types
namespace System {

	typedef unsigned char Byte;

	/// a typedef to std::vector<unsigned char>
	typedef gsstl::vector<Byte> Bytes;

} /* namespace System */

#endif /* _SYSTEM_BYTES_HPP_INCLUDED_ */
