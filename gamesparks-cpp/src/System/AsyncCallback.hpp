#ifndef _SYSTEM_ASYNCCALLBACK_HPP_
#define _SYSTEM_ASYNCCALLBACK_HPP_

#include <functional>
#include "IAsyncResult.hpp"
#include <GameSparks/gsstl.h>

namespace System {

    typedef gsstl::function<void (const IAsyncResult& ar)> AsyncCallback;

}

#endif /* _SYSTEM_ASYNCCALLBACK_HPP_ */
