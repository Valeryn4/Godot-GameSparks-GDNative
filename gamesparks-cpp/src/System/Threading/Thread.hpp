#ifndef _SYSTEM_THREADING_THREAD_HPP_INCLUDED_
#define _SYSTEM_THREADING_THREAD_HPP_INCLUDED_

#include <GameSparks/gsstl.h>

namespace System { namespace Threading {

	class Thread
	{
		public:
		    static void SetName(gsstl::string name);
			static gsstl::string GetName();
		protected:

		private:
	};

}} /* namespace System.Threading */

#endif /* _SYSTEM_THREADING_THREAD_HPP_INCLUDED_ */
