#ifndef _SYSTEM_NOTSUPPORTEDEXCEPTION_HPP_INCLUDED_
#define _SYSTEM_NOTSUPPORTEDEXCEPTION_HPP_INCLUDED_

#include "SystemException.hpp"

namespace System {

	class NotSupportedException : public SystemException
	{
		public: NotSupportedException(const gsstl::string what) : SystemException(what, "System.NotSupportedException") {}
		public: NotSupportedException(const gsstl::string what, const gsstl::string typeName) : SystemException(what, typeName) {}
	};

} /* namespace System */

#endif /* _SYSTEM_NOTSUPPORTEDEXCEPTION_HPP_INCLUDED_ */
