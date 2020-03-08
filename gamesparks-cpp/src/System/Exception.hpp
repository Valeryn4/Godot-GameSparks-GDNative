#ifndef _GAMESPARKSRT_EXCEPTION_HPP_
#define _GAMESPARKSRT_EXCEPTION_HPP_

#include "../../include/GameSparks/gsstl.h"

namespace System {

	class Exception : public gsstl::runtime_error
	{
		public:
			Exception(const gsstl::string& what) : gsstl::runtime_error(what.c_str()), typeName("System.Exception") {}
			Exception(const gsstl::string& what, const gsstl::string& typeName_) : gsstl::runtime_error(what.c_str()), typeName(typeName_) {}

			void PushFrame(const gsstl::string& file, int line, const gsstl::string& function)
			{
				callStack.push_back(Frame(file, line, function));
			}

			friend gsstl::ostream& operator << (gsstl::ostream& os, const Exception& e)
			{
				os << "Traceback (most recent call last):" << gsstl::endl;
				for(const auto& frame : e.callStack)
				{
					os << "  " << frame << gsstl::endl;
				}
				os << e.typeName << ": " << e.what() << gsstl::endl;

				return os;
			}

			gsstl::string Format() const
			{
				gsstl::stringstream ss;
				ss << *this;
				return ss.str();
			}
		private:
			const gsstl::string typeName;

			struct Frame
			{
				Frame(const gsstl::string& file_, const int line_, const gsstl::string function_)
				:file(file_), line(line_), function(function_) {}

				const gsstl::string file;
				const int line = -1;
				const gsstl::string function;

				friend gsstl::ostream& operator << (gsstl::ostream& os, const Frame& frame)
				{
					os << frame.file << ":" << frame.line << " in " << frame.function;
					return os;
				}
			};
			typedef gsstl::list<Frame> CallStack;
			CallStack callStack;
	};

} /* namespace System */

#endif /* _GAMESPARKSRT_EXCEPTION_HPP_ */
