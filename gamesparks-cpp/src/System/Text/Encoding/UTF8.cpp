#include <iterator>

#include "./UTF8.hpp"

namespace System { namespace Text { namespace Encoding {

gsstl::string UTF8::GetString(const System::Bytes& in)
{
    gsstl::string out;
    gsstl::copy(in.begin(), in.end(), gsstl::back_inserter(out));
    return out;
}

gsstl::string UTF8::GetString(const System::Bytes& in, int index, int count)
{

    assert(index < static_cast<int>(in.size()));
    assert(count <= static_cast<int>(in.size())-index);

    gsstl::string out;
    gsstl::copy(in.begin()+index, in.begin()+index+count, gsstl::back_inserter(out));
    return out;
}

System::Bytes UTF8::GetBytes(const gsstl::string& in)
{
    System::Bytes out;
    gsstl::copy(in.begin(), in.end(), gsstl::back_inserter(out));
    return out;
}

}}} /* namespace System.Text.Encoding */
