#ifndef _SYSTEM_TEXT_ENCODING_UTF8_HPP_INCLUDED_
#define _SYSTEM_TEXT_ENCODING_UTF8_HPP_INCLUDED_

//#include <string>
//#include <vector>
//#include <cassert>
#include "../../../../include/System/Bytes.hpp"

namespace System { namespace Text { namespace Encoding {

	class UTF8
	{
		public:
			static gsstl::string GetString(const System::Bytes& in);
			static gsstl::string GetString(const System::Bytes& in, int index, int count);
			static System::Bytes GetBytes(const gsstl::string& in);
	};

}}} /* namespace System.Text.Encoding */

#endif /* _SYSTEM_TEXT_ENCODING_UTF8_HPP_INCLUDED_ */
