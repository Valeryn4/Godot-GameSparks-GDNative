#include "./CustomRequest.hpp"

namespace GameSparks { namespace RT {

CustomRequest::CustomRequest(int opCode_, GameSparksRT::DeliveryIntent intent_, const System::ArraySegment<System::Byte>& payload_, const RTData& data, gsstl::vector<int> targetPlayers)
:Commands::RTRequest(opCode_)
{
    payload.resize(payload_.Count());

    gsstl::copy(
        payload_.Array().begin() + payload_.Offset(),
        payload_.Array().begin() + payload_.Offset() + payload_.Count(),
        payload.begin());

    intent = intent_;
    Data = data;
    if(!targetPlayers.empty())
        gsstl::copy(targetPlayers.begin(), targetPlayers.end(), gsstl::back_inserter(TargetPlayers));
}

System::Failable<void> CustomRequest::Serialize(System::IO::Stream &stream) const {
    if (!payload.empty()) {
        GS_CALL_OR_THROW(stream.Write (payload, 0, int(payload.size())));
    }
    return {};
}

}} /* namespace GameSparks.RT */
