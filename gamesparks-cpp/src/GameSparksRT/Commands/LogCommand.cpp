#include "./LogCommand.hpp"
#include "../../System/Threading/Thread.hpp"
//#include <thread>
//#include <sstream>

namespace GameSparks { namespace RT {

        LogCommand::LogCommand(const gsstl::string &tag_, GameSparks::RT::GameSparksRT::LogLevel level_,
                               const gsstl::string &msg_)
        :tag(tag_), msg(msg_), level(level_)  {}

        void LogCommand::Execute() {
            if(GameSparksRT::ShouldLog(tag, level))
            {
                gsstl::stringstream tid;
                tid << System::Threading::Thread::GetName() << "(" << gsstl::this_thread::get_id() << ")";
                GameSparksRT::Logger (tid.str() + " " + tag + ":" + msg);
            }
        }

}} /* namespace GameSparks.RT */

	
