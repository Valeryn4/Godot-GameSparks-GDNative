#ifndef _GAMESPARKSRT_LOGCOMMAND_HPP_
#define _GAMESPARKSRT_LOGCOMMAND_HPP_

#include "../IRTCommand.hpp"
//#include <string>
#include "../../../include/GameSparksRT/GameSparksRT.hpp"
//#include <GameSparksRT/GameSparksRT.hpp>

namespace GameSparks { namespace RT {

	class LogCommand : public IRTCommand
	{
		public:
			LogCommand(const gsstl::string& tag, GameSparks::RT::GameSparksRT::LogLevel level, const gsstl::string& msg);
			virtual void Execute() override;
		private:
			const gsstl::string tag, msg;
			const GameSparks::RT::GameSparksRT::LogLevel level;
	};

}} /* namespace GameSparks.RT */

#endif /* _GAMESPARKSRT_LOGCOMMAND_HPP_ */
