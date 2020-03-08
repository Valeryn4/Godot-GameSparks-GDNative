// Copyright 2015 GameSparks Ltd 2015, Inc. All Rights Reserved.

#include <GameSparks/GSVersion.h>

#if defined(WINAPI_FAMILY)
#	define __STDC_WANT_SECURE_LIB__ 0
#	define sprintf_s(buf, size, fmt, ...) sprintf(buf, fmt, __VA_ARGS__)
#	define swprintf_s(buf, size, fmt, ...) swprintf(buf, size, fmt, __VA_ARGS__)
#endif

#include "./GSClientConfig.h"

#include <GameSparks/GS.h>
//#include <iostream>
#include <GameSparks/GSUtil.h>
//#include <sstream>
#include <GameSparks/GSMessage.h>
//#include <cassert>
//#include <ctime>
#include <easywsclient/easywsclient.hpp>

#include <stdio.h>
#if (defined(WIN32) || defined(WINAPI_FAMILY)) && !defined(IW_SDK) && !defined(snprintf)
#	define snprintf _snprintf
#endif /* WIN32 */

using namespace GameSparks;
using namespace GameSparks::Core;
using namespace GameSparks::Api::Messages;

/*__declspec(dllexport) GS GameSparks::Core::GS = GS();*/

GS::GS()
    : GameSparksAvailable()
    , GameSparksAuthenticated()
    , OnNonce()
    , OnPersistentQueueLoadedCallback()
    , m_GSPlatform(NULL)
    , m_RequestCounter(0)
    , m_Ready(false)
    , m_Paused(false)
    , m_Initialized(false)
    , m_durableQueuePaused(false)
    , m_durableQueueRunning(true)
    , m_SessionId("")
	, m_connectionAttempts(1)
	, m_mustBeConnectedIn(0.0f)
    , m_sendNextDurableRequestIn(0.0f)
{
	/*
		If this assertion fails, your compiler fails to initialize
		GameSparksAvailable, even though a default initialization was
		requested in the initializer list.
	*/
	assert( !GameSparksAvailable );
	assert( !GameSparksAuthenticated );
}

GS::~GS()
{
	for (t_ConnectionContainer::iterator it = m_Connections.begin(); it != m_Connections.end(); ++it)
	{
		// this is to avoid the assert in the destructor of GSConnection firing.
		(*it)->m_PendingRequests.clear();
		delete *it;
	}
	m_Connections.clear();

	ClearAllMessageListeners();
}

void GS::ClearAllMessageListeners()
{
	for(t_MessageHandlerMap::iterator i = m_MessageHandlers.begin(); i != m_MessageHandlers.end(); ++i)
	{
		delete i->second;
	}
	m_MessageHandlers.clear();
}

void GS::Initialise(IGSPlatform* gSPlatform)
{
	m_Initialized = true;
	m_Paused = false;
	m_GSPlatform = gSPlatform;
	m_ServiceUrl = buildServiceUrl(m_GSPlatform);

	GSClientConfig::instance().deserialze(gSPlatform->LoadValue("clientConfig"));
    m_mustBeConnectedIn =
        GSClientConfig::instance().ComputeSleepPeriod(m_connectionAttempts) +
        GSClientConfig::instance().getHandshakeOffset();

	gSPlatform->DurableInit();
    InitialisePersistentQueue();
    SetDurableQueueRunning(true);

	m_Connections.push_back(new GSConnection(this, m_GSPlatform));
	DebugLog("Initialized");
}

void GameSparks::Core::GS::ShutDown()
{
	//DebugLog("Shutdown");
	m_Initialized = false;
	m_Paused = true;
	Stop(true);
	// clear the connections
	//UpdateConnections(0);
}

void GameSparks::Core::GS::NewConnection()
{
	GS_CODE_TIMING_ASSERT();
	if (!m_Paused)
	{
		DebugLog("Create new connection");
		Stop(false);

        m_connectionAttempts++;
        m_mustBeConnectedIn =
            GSClientConfig::instance().ComputeSleepPeriod(m_connectionAttempts) +
            GSClientConfig::instance().getHandshakeOffset();

        gsstl::reverse(m_Connections.begin(), m_Connections.end());
		m_Connections.push_back(new GSConnection(this, m_GSPlatform));
		gsstl::reverse(m_Connections.begin(), m_Connections.end());
    }
}

void GS::Stop(bool termiante)
{
	for (t_ConnectionContainer::iterator it = m_Connections.begin(); it != m_Connections.end(); ++it)
	{
		GSConnection* connection = *it;
		if (termiante) connection->Terminate();
		else connection->Stop();
	}


	SetAvailability(false);
}

void GS::OnMessageReceived(const gsstl::string& message, GSConnection& connection)
{
	GS_CODE_TIMING_ASSERT();

	GSObject response = GSObject::FromJSON(message);

	if (response.ContainsKey("connectUrl"))
	{
		DebugLog("Received new connection url from gamesparks backend. Establishing new connection now.");
		m_ServiceUrl = response.GetString("connectUrl").GetValue();
		NewConnection();
	}

	if (response.ContainsKey("authToken"))
	{
		DebugLog("Received auth token");
		m_GSPlatform->SetAuthToken(response.GetString("authToken").GetValue());
	}

	if (response.GetType().HasValue())
	{
		if (response.GetType().GetValue() == ".AuthenticatedConnectResponse")
		{
			DebugLog("Handle authentication connect response with immediate handshake");
			Handshake(response, connection);
			GSClientConfig::instance().setFromObject(response.GetGSDataObject("clientConfig").GetValueOrDefault({}));
			m_GSPlatform->StoreValue("clientConfig", GSClientConfig::instance().serialize());
		}
		else
		{
			ProcessReceivedItem(response, &connection);
		}
	}


	//std::cout << "@class: " << response.GetType().first.c_str() << std::endl;
	//std::cout << "@class: " << response.GetJSON().c_str() << std::endl;
}



void GameSparks::Core::GS::Handshake(GSObject& response, GSConnection& connection)
{
	GS_CODE_TIMING_ASSERT();

	if (response.ContainsKey("error"))
	{
		// this method can be called indirectly from the websockets _dispatch member function
		// if we'd call shutdown here, we'd delete the socket, while a member function of of the
		// web socket is still on the callstack. Therefore we defer the shutdown
		DebugLog("Got error during handshake. Please make sure, that you've setup you credentials.");
	}
	else if (response.ContainsKey("nonce"))
	{
		SendHandshake(response, connection);
	}
	else
	{
		if (response.ContainsKey("sessionId"))
		{
			m_SessionId = response.GetString("sessionId").GetValue();
			if (response.ContainsKey("authToken"))
			{
				m_GSPlatform->SetAuthToken(response.GetString("authToken").GetValue());
			}
			else
			{
				m_GSPlatform->SetAuthToken("0");
			}
			connection.SetReady(true);
            m_connectionAttempts = 1;
			SetAvailability(true);

			if (response.ContainsKey("userId"))
			{
				SetUserId(response.GetString("userId").GetValue());
			}
		}
	}
}

void GameSparks::Core::GS::SendHandshake(GSObject& response, GSConnection& connection)
{
	GSRequest handshakeRequest(*this, ".AuthenticatedConnectRequest");

	gsstl::string nonce = response.GetString("nonce").GetValue();

	gsstl::string hmac;
	if(OnNonce)
		hmac = OnNonce(nonce);
	else
		hmac = GameSparks::Util::getHMAC(nonce, m_GSPlatform->GetGameSparksSecret());

	handshakeRequest.AddString("hmac", hmac);
	handshakeRequest.AddString("os", m_GSPlatform->GetDeviceOS());
	handshakeRequest.AddString("platform", m_GSPlatform->GetPlatform());
	handshakeRequest.AddString("deviceId", m_GSPlatform->GetDeviceId());

	{ // add device stats to AuthenticatedConnectRequest
		static gsstl::map<gsstl::string, gsstl::string> rawDeviceStats = m_GSPlatform->GetDeviceStats();
		GSRequestData deviceStats;

		for (const auto & kv : rawDeviceStats)
		{
			if (!kv.first.empty() && !kv.second.empty())
			{
				deviceStats.AddString(kv.first, kv.second);
			}
		}

		deviceStats.AddString("gssdk", GS_SDK_VERSION);

		handshakeRequest.AddObject("deviceStats", deviceStats);
	}
	
	if (m_GSPlatform->GetAuthToken() != "" && m_GSPlatform->GetAuthToken() != "0")
	{
		handshakeRequest.AddString("authToken", m_GSPlatform->GetAuthToken());
	}

	if (m_SessionId != "")
	{
		handshakeRequest.AddString("sessionId", m_SessionId);
	}

	handshakeRequest.m_expiresInSeconds = m_mustBeConnectedIn;
	connection.SendImmediate(handshakeRequest);
	DebugLog("Handshake request sent");
}

void GameSparks::Core::GS::SendDurable(GSRequest& request)
{
	request.AddString("requestId", GetUniqueRequestId(true));
    request.m_durableAttempts = 0;
	request.m_expiresInSeconds = 0.0f;//GSClientConfig::instance().getRequestTimeout() + GSClientConfig::instance().ComputeSleepPeriod(request.m_durableAttempts);
	m_PersistentQueue.push_front(request);
	WritePersistentQueue();
}

void GameSparks::Core::GS::Send(GSRequest& request)
{
    assert(request.m_expiresInSeconds > Seconds(0));
	if (request.GetDurable())
	{
		SendDurable(request);
		return;
	}

	if (m_Connections.size() == 0)
	{
		NewConnection();
	}

	if (m_Connections.size() > 0 && m_Connections[0]->GetReady())
	{
		m_Connections[0]->SendImmediate(request);
	}
	else
	{
		m_SendQueue.push_back(request);
	}
}

void GS::Update(Seconds deltaTimeInSeconds)
{
	GS_CODE_TIMING_ASSERT();
	if (m_Initialized)
	{
		m_mustBeConnectedIn -= deltaTimeInSeconds;
		UpdateConnections(deltaTimeInSeconds);
		ProcessQueues(deltaTimeInSeconds);
	}
}

void GameSparks::Core::GS::DebugLog(const gsstl::string& message)
{
	GS_CODE_TIMING_ASSERT();
	m_GSPlatform->DebugMsg(message);
}

void GS::UpdateConnections(Seconds deltaTimeInSeconds)
{
	GS_CODE_TIMING_ASSERT();
	for (t_ConnectionContainer::size_type i = 0; i < m_Connections.size(); ++i)
	{
		GSConnection* connection = m_Connections[i];

		if(connection->IsWebSocketConnectionAlive() && !connection->Update(deltaTimeInSeconds))
			break;
	}
}

gsstl::string GameSparks::Core::GS::GetUniqueRequestId(bool durable)
{
	GS_CODE_TIMING_ASSERT();
	static char buffer[256];
	snprintf(buffer, sizeof(buffer)/sizeof(buffer[0]), "%ld_%ld", (long)time(0), m_RequestCounter++);
	if (durable)
	{
		return "d_" + gsstl::string(buffer);
	}else
	{
		return gsstl::string(buffer);
	}

}

void GameSparks::Core::GS::OnWebSocketClientError(const easywsclient::WSError& error, GSConnection* connection)
{
	/*if(error.code == easywsclient::WSError::DNS_LOOKUP_FAILED)
	{
		Disconnect();
	}

	if(m_Connections.size() > 0 && m_Connections[0] == connection)
	{
		SetAvailability(false);

	}*/

	if(!m_Connections.empty() && m_Connections.front() == connection)
	{
        //Reset the url to the load balancer url in case the server being connected to no longer exsts
        m_ServiceUrl = buildServiceUrl(m_GSPlatform);

		DebugLog("Received websocket error: " + error.message);
		DebugLog("Got websocket error. Please make sure, that you've setup you credentials.");

		SetAvailability(false);
	}
}

void GameSparks::Core::GS::SetAvailability(bool available)
{
	if (m_Ready != available)
	{
		m_Ready = available;
		
		if (GameSparksAvailable)
		{
			GameSparksAvailable(*this, available);
		}
	}
}

void GameSparks::Core::GS::ConnectIfRequired()
{
	GS_CODE_TIMING_ASSERT();
	if (m_mustBeConnectedIn <= Seconds(0.0) &&
		(m_Connections.size() == 0 ||
		m_Connections[0]->m_Stopped) )
	{
		NewConnection();
	}
}

bool GameSparks::Core::GS::GetAuthenticated() const
{
	return m_Ready && m_GSPlatform->GetAuthToken() != "" && m_GSPlatform->GetAuthToken() != "0";
}

void GameSparks::Core::GS::ProcessSendQueue(Seconds deltaTimeInSeconds)
{
	if (m_SendQueue.size() > 0)
	{
		GSRequest& request = *m_SendQueue.begin();

		request.m_expiresInSeconds -= deltaTimeInSeconds;

		if (request.m_expiresInSeconds <= 0)
		{
			CancelRequest(request); // needs to be called, before it's popped from the queue
			m_SendQueue.pop_front();
		}

		if (m_Connections.size() > 0 && m_Connections[0]->GetReady())
		{
			m_Connections[0]->SendImmediate(request);
			m_SendQueue.pop_front();
		}
	}
}

void GameSparks::Core::GS::ProcessPendingQueue(Seconds deltaTimeInSeconds)
{
	for(t_ConnectionContainer::iterator connection = m_Connections.begin(); connection != m_Connections.end(); ++connection)
	{
		gsstl::vector<GSRequest> toCancel;
		for(GSConnection::t_RequestMap::iterator request=(*connection)->m_PendingRequests.begin(); request!=(*connection)->m_PendingRequests.end(); ++request)
		{
			request->second.m_expiresInSeconds -= deltaTimeInSeconds;

			if(request->second.m_expiresInSeconds < Seconds(0))
			{
				toCancel.push_back(request->second);
			}
		}

		for(gsstl::vector<GSRequest>::iterator request = toCancel.begin(); request != toCancel.end(); ++request)
		{
			CancelRequest(*request, *connection);
		}
	}
}

void GameSparks::Core::GS::CancelRequest(GSRequest& request)
{
	GSObject error("ClientError");
	error.AddObject("error", GSRequestData().AddString("error", "timeout"));
	error.AddString("requestId", request.GetString("requestId").GetValue());

	request.Complete(error);
}

void GameSparks::Core::GS::CancelRequest(GSRequest& request, GSConnection* connection)
{
	GSObject error("ClientError");
	error.AddObject("error", GSRequestData().AddString("error", "timeout"));
	error.AddString("requestId", request.GetString("requestId").GetValue());
	ProcessReceivedResponse(error, connection);
}

void GameSparks::Core::GS::ProcessQueues(Seconds deltaTimeInSeconds)
{
	GS_CODE_TIMING_ASSERT();
	ConnectIfRequired();

	TrimOldConnections();
	ProcessPersistentQueue(deltaTimeInSeconds);
	ProcessSendQueue(deltaTimeInSeconds);
	ProcessPendingQueue(deltaTimeInSeconds);
}

void GameSparks::Core::GS::TrimOldConnections()
{
	GS_CODE_TIMING_ASSERT();
	for (t_ConnectionContainer::iterator it = m_Connections.begin(); it != m_Connections.end(); )
	{
		GSConnection* connection = *it;

        bool deleteConnection = false;

        if(m_mustBeConnectedIn < Seconds(0) && !connection->GetReady())
        {
            DebugLog("Connection not ready in time, deleting it");
            deleteConnection = true;
        }

		if (connection->m_PendingRequests.size() == 0 && connection->m_Stopped)
        {
            DebugLog("Connection stopped, deleting it");
            deleteConnection = true;
        }

        if(deleteConnection)
		{
			connection->Close();
			it = m_Connections.erase(it);
			delete connection;
		}
		else
		{
			++it;
		}
	}
}

void GameSparks::Core::GS::ProcessReceivedResponse(const GSObject& response, GSConnection* connection)
{
	if (response.GetString("requestId").HasValue())
	{
		gsstl::string requestId = response.GetString("requestId").GetValue();
		
		GSConnection::t_RequestMap::iterator findIt = connection->m_PendingRequests.find(requestId);
		if (findIt != connection->m_PendingRequests.end())
		{
			GSRequest request = findIt->second;
			connection->m_PendingRequests.erase(findIt);

			if (request.GetDurable())
			{
				// remove from persistent queue
				//It's durable request, if it's a ClientError do nothing as it will be retried
                if (response.GetString("@class").GetValueOrDefault("<none>") != "ClientError")
                {
					RemoveDurableQueueEntry(request);
					request.Complete(response);
                }
			}
			else
			{
				request.Complete(response);
			}
		}
	}
}

bool GameSparks::Core::GS::RemoveDurableQueueEntry(const GSRequest& request)
{
	gsstl::string idToRemove = request.GetString("requestId").GetValue();
	for (t_PersistentQueue::iterator it = m_PersistentQueue.begin(); it != m_PersistentQueue.end(); it++)
	{
		if (it->GetString("requestId").GetValue() == idToRemove)
		{
			m_PersistentQueue.erase(it);
			WritePersistentQueue();
			m_GSPlatform->DebugMsg("Removed request from persistent queue");

			return true;
		}
	}
	return false;
}

size_t GameSparks::Core::GS::GetRequestQueueCount()
{
	return m_PersistentQueue.size();
}

bool GameSparks::Core::GS::GetDurableQueueRunning()
{
	return m_durableQueueRunning;
}
void GameSparks::Core::GS::SetDurableQueueRunning(bool value)
{
	m_durableQueueRunning = value;
}

bool StringEndsWith(const gsstl::string& str, const gsstl::string& pattern)
{
	return  (str.size() >= pattern.size() && str.substr(str.size() - pattern.size()) == pattern);
}

void GameSparks::Core::GS::ProcessReceivedItem(const GSObject& response, GSConnection* connection)
{
	GS_CODE_TIMING_ASSERT();
	gsstl::string responseType = response.GetType().GetValue();
	
	if (StringEndsWith(responseType, "Response"))
	{
		ProcessReceivedResponse(response, connection);
		if (responseType == ".AuthenticationResponse")
		{
			SetUserId(response.GetString("userId").GetValue());
		}
	}
	else if (StringEndsWith(responseType, "Message"))
	{
        t_MessageHandlerMap::iterator pos = m_MessageHandlers.find(responseType);
        
        if (pos != m_MessageHandlers.end()){
            pos->second->CallMessageListener(*this, response);
        }
        //GSMessage::NotifyHandlers(response);
	}
}

void GameSparks::Core::GS::ProcessPersistentQueue(Seconds deltaTimeInSeconds)
{
	GS_CODE_TIMING_ASSERT();
    m_sendNextDurableRequestIn -= deltaTimeInSeconds;

	if(!GetDurableQueueRunning() || m_durableQueuePaused)
	{
		return;
	}

	int durableRequestsInFlight = 0;
	for(const auto& connection : m_Connections)
	{
		for(const auto& request : connection->m_PendingRequests)
		{
			if(request.second.m_Durable)
				durableRequestsInFlight++;
		}
	}

    for (auto& request : m_PersistentQueue)
    {
        request.m_expiresInSeconds -= deltaTimeInSeconds;
    }

	for (auto& request : m_PersistentQueue)
	{
        if(durableRequestsInFlight >= GSClientConfig::instance().getDurableConcurrentRequests())
            break; // to many durable requests in flight, bail out

        if(m_sendNextDurableRequestIn > 0.0f)
            break; // respect drain interval

		if (request.m_expiresInSeconds <= 0)
		{
			if (m_Connections.size() > 0 && m_Connections[0]->GetReady())
			{
				request.m_durableAttempts++;
				request.m_expiresInSeconds = GSClientConfig::instance().getRequestTimeout() + GSClientConfig::instance().ComputeSleepPeriod(request.m_durableAttempts);
				m_Connections[0]->SendImmediate(request);
                durableRequestsInFlight++;
                m_sendNextDurableRequestIn = GSClientConfig::instance().getDurableDrainInterval();
            }
		}
	}
}

void GameSparks::Core::GS::SetUserId(const gsstl::string& userId)
{
	GS_CODE_TIMING_ASSERT();

	// Note: the .NET SDK does not check if the users are different and executes the queue init on the main thread
	if(m_GSPlatform->GetUserId() != userId)
	{
		// clear the pending durable requests for recent user.
		DebugLog("New UserId init persistent queue");

		bool previous_durableQueuePaused = m_durableQueuePaused;

		//Temporarily stop durable queue processing
		m_durableQueuePaused = true;

		m_GSPlatform->SetUserId(userId);

		InitialisePersistentQueue();

		//We want this to be callback to the user code to allow them to make decisions
		//about the queue before we start processing it, but after it's been initialised
		if (GameSparksAuthenticated)
			GameSparksAuthenticated(*this, userId);

		//Resume queue processing.
		m_durableQueuePaused = previous_durableQueuePaused;
	}
}


void GameSparks::Core::GS::Disconnect()
{
	GS_CODE_TIMING_ASSERT();
	m_Paused = true;
	Stop(false);
}

void GameSparks::Core::GS::Reconnect()
{
	GS_CODE_TIMING_ASSERT();
	m_Paused = false;
	ConnectIfRequired();
}

void GameSparks::Core::GS::NetworkChange(bool available)
{
	GS_CODE_TIMING_ASSERT();
	if (available)
	{
		Disconnect();
	}
	else
	{
		Reconnect();
	}
}

void GameSparks::Core::GS::Reset()
{
	GS_CODE_TIMING_ASSERT();
	Disconnect();
	m_SessionId = "";
	m_GSPlatform->SetAuthToken("0");
	Reconnect();
}


//! save requests queue as name
void GS::WritePersistentQueue()
{
	GS_CODE_TIMING_ASSERT();
	gsstl::string json = SerializeRequestQueue(m_PersistentQueue);
	m_GSPlatform->StoreValue(m_GSPlatform->GetUserId() + "_persistentQueue", json);
}

//! an empty queue will be returned, if no queue named queue can be found
void GS::InitialisePersistentQueue()
{
	GS_CODE_TIMING_ASSERT();
	gsstl::string json = m_GSPlatform->LoadValue( m_GSPlatform->GetUserId() + "_persistentQueue");
	m_PersistentQueue = DeserializeRequestQueue(json);
    
    if (OnPersistentQueueLoadedCallback)
    {
        OnPersistentQueueLoadedCallback(*this);
    }
}


gsstl::string GS::SerializeRequestQueue(const t_PersistentQueue& q)
{
	GS_CODE_TIMING_ASSERT();
	cJSON* list = cJSON_CreateArray();

	for (t_PersistentQueue::const_iterator i = q.begin(); i != q.end(); ++i)
	{
		cJSON* item = i->GetBaseData();
		cJSON_AddItemReferenceToArray(list, item);
	}

	char* asText = cJSON_Print(list);
	gsstl::string result(asText);
	free(asText);
	cJSON_Delete(list);

	return result;
}


GS::t_PersistentQueue GS::DeserializeRequestQueue(const gsstl::string& s)
{
	GS_CODE_TIMING_ASSERT();
	cJSON* list = cJSON_Parse(s.c_str());

	t_PersistentQueue result;
	if (list)
	{
		int size = cJSON_GetArraySize(list);
		for (int i = 0; i < size; ++i)
		{
			if (cJSON* item = cJSON_GetArrayItem(list, i))
			{
				GSRequest request(*this, item);
                request.SetDurable(true); // every request from the queue is durable!
				request.m_durableAttempts = 1;
				request.m_expiresInSeconds = 0;
				result.push_back(request);
			}
		}

		cJSON_Delete(list);
	}

	return result;
}


GS::t_PersistentQueue& GS::GetDurableQueueEntries()
{
	return m_PersistentQueue;
}

struct UserDataEqualTo
{
	UserDataEqualTo(const void* userData) : userData_(userData) {}
	bool operator()(const GSRequest& request) { return request.GetUserData() == userData_; }
	bool operator()(const gsstl::pair<gsstl::string, GSRequest>& p) { return p.second.GetUserData() == userData_; }
	const void* userData_;
};

void GS::ChangeUserDataForRequests(const void *from, void* to)
{
	GS_CODE_TIMING_ASSERT();
	assert(m_Initialized);
	assert(this->m_GSPlatform);

	// erase from send queue
	for (t_SendQueue::iterator i = m_SendQueue.begin(); i != m_SendQueue.end(); ++i)
	{
		if (i->GetUserData() == from)
		{
			i->SetUserData(to);
		}
	}

	for (t_PersistentQueue::iterator i = m_PersistentQueue.begin(); i != m_PersistentQueue.end(); ++i)
	{
		if (i->GetUserData() == from)
		{
			i->SetUserData(to);
		}
	}

	// also change it it on all pending requests in all connections
	for (t_ConnectionContainer::iterator i = m_Connections.begin(); i != m_Connections.end(); ++i)
	{
		for (GSConnection::t_RequestMap::iterator j = (*i)->m_PendingRequests.begin(); j != (*i)->m_PendingRequests.end(); ++j)
		{
			if (j->second.GetUserData() == from)
			{
				j->second.SetUserData(to);
			}
		}
	}
}



gsstl::string GS::buildServiceUrl(const IGSPlatform* platform)
{
    #if defined(GS_SDK_TEST_WEBSOCKET_ENDPOINT)
        return GS_SDK_TEST_WEBSOCKET_ENDPOINT + platform->m_apiKey;
    #else
	gsstl::string credential = platform->m_apiCredential.empty() ? "device" : platform->m_apiCredential;
	gsstl::string apiDomain = platform->m_apiDomain.empty() ? "ws.gamesparks.net" : platform->m_apiDomain;
    
	if (platform->GetGameSparksSecret().find(':') != gsstl::string::npos)
	{
		credential = "secure";
	}

	return "wss://" + platform->m_apiStage + "-" + platform->m_apiKey + "." + apiDomain + "/ws/" + credential + "/" + platform->m_apiKey;
    #endif
}
