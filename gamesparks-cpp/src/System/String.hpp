#ifndef _SYSTEM_STRING_HPP_INCLUDED_
#define _SYSTEM_STRING_HPP_INCLUDED_

//#include <string>
//#include <sstream>
//#include <iomanip>

namespace System {

	class String
	{
		public:

		template <typename T>
		static gsstl::string ToString(const T& o)
        {
            gsstl::stringstream stream;
            stream << gsstl::boolalpha;
            stream << o;
            return stream.str();
        }
	};

} /* namespace System */

#endif /* _SYSTEM_STRING_HPP_INCLUDED_ */
