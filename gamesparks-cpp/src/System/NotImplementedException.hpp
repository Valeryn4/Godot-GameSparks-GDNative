#ifndef _SYSTEM_NOTIMPLEMENTEDEXCEPTION_HPP_INCLUDED_
#define _SYSTEM_NOTIMPLEMENTEDEXCEPTION_HPP_INCLUDED_

#include "./Exception.hpp"

namespace System {

	class NotImplementedException : public Exception
	{
		public: NotImplementedException(const gsstl::string what) : Exception(what, "NotImplementedException") {}
	};

} /* namespace System */

#endif /* _SYSTEM_NOTIMPLEMENTEDEXCEPTION_HPP_INCLUDED_ */
