
#include "../../include/GameSparksRT/GameSparksRT.hpp"
//#include <ostream>
//#include <iostream>
#include "./RTSessionImpl.hpp"

namespace GameSparks { namespace RT {

/// Constructor
GameSparksRTSessionBuilder::GameSparksRTSessionBuilder()
:pimpl(new Pimpl()) {}

/// Destructor
GameSparksRTSessionBuilder::~GameSparksRTSessionBuilder()
{
	delete pimpl;
}


GameSparksRTSessionBuilder& GameSparksRTSessionBuilder::SetConnectToken(const gsstl::string& connectToken_){
    this->pimpl->connectToken = connectToken_;
    return *this;
}

GameSparksRTSessionBuilder& GameSparksRTSessionBuilder::SetHost(const gsstl::string& host_){
    this->pimpl->host = host_;
    return *this;
}

/// sets TcpPort and FastPort to the same value
GameSparksRTSessionBuilder& GameSparksRTSessionBuilder::SetPort(const gsstl::string& port_){
    this->pimpl->port = port_;
    return *this;
}

GameSparksRTSessionBuilder& GameSparksRTSessionBuilder::SetPort(int _port) {
	assert(_port > 0);
	assert(_port <= 65535);

	gsstl::stringstream ss;
	ss << _port;
	this->pimpl->port = ss.str();
	return *this;
}

GameSparksRTSessionBuilder& GameSparksRTSessionBuilder::SetListener(IRTSessionListener* listener_){

    // if this assertion fails, you have already registered a session listener.
    assert(this->pimpl->listener == nullptr || listener_ == nullptr);

    this->pimpl->listener = listener_;
    return *this;
}

/// you own the return value. make sure to put it into a std::unique_ptr or std::shared_ptr (or delete it manually).
IRTSession* GameSparksRTSessionBuilder::Build() const{
    IRTSession* session = new RTSessionImpl (pimpl->connectToken, pimpl->host, pimpl->port, pimpl->port);
    session->SessionListener = pimpl->listener;
    if(pimpl->listener)
		pimpl->listener->session = session;
    return session;
}

// not a member variable to avoid DLL linking issues
static gsstl::map<gsstl::string, GameSparksRT::LogLevel> tagLevels;

gsstl::function<void (const gsstl::string&)> GameSparksRT::Logger = [](const gsstl::string& msg){
    gsstl::clog << msg << gsstl::endl;
};

GameSparksRT::LogLevel GameSparksRT::logLevel = GameSparksRT::LogLevel::LL_ERROR;


void GameSparksRT::SetRootLogLevel(GameSparksRT::LogLevel level) {
    logLevel = level;
}

void GameSparksRT::SetLogLevel(const gsstl::string &tag, GameSparksRT::LogLevel level) {
    tagLevels.insert(gsstl::make_pair(tag, level));
}

bool GameSparksRT::ShouldLog(const gsstl::string &tag, GameSparksRT::LogLevel level) {
    auto pos = tagLevels.find(tag);
    if(pos != tagLevels.end()){
        return pos->second >= level;
    }
    return logLevel >= level;
}

GameSparksRTSessionBuilder GameSparksRT::SessionBuilder() {
    return RT::GameSparksRTSessionBuilder();
}

}} /* namespace GameSparks.RT */
