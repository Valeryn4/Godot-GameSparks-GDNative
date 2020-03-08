#include <GameSparks/GSPlatformDeduction.h>

#if (((GS_TARGET_PLATFORM == GS_PLATFORM_WIN32) && !GS_WINDOWS_DESKTOP) || GS_TARGET_PLATFORM == GS_PLATFORM_XBOXONE)


#include <easywsclient/easywsclient.hpp>
#include <GameSparks/gsstl.h>

#include <windows.networking.h>
#include <ppltasks.h>
#include <mutex>
#include <thread>
#include <queue>
#include <cassert>

namespace {
	using namespace Windows::Networking::Sockets;
	//using namespace Windows::Storage::Streams;
	//using namespace Windows::Foundation;
	//using namespace Platform;

	static gsstl::wstring utf8_to_wstring(const gsstl::string& str)
	{

		int output_size = MultiByteToWideChar(CP_UTF8, NULL, str.c_str(), static_cast<int>(str.size()), NULL, 0);
		assert(output_size > 0);
		gsstl::vector<wchar_t> buffer(output_size);
		int result = MultiByteToWideChar(CP_UTF8, NULL, str.c_str(), static_cast<int>(str.size()), buffer.data(), static_cast<int>(buffer.size()));
		(void)result; // unused in release builds
		assert(result == output_size);
		gsstl::wstring ret(buffer.begin(), buffer.end());
		return ret;
	}

	// convert wstring to UTF-8 string
	static gsstl::string wstring_to_utf8(const gsstl::wstring& str)
	{
		int output_size = WideCharToMultiByte(CP_UTF8, NULL, str.c_str(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
		assert(output_size > 0);
		gsstl::vector<char> buffer(output_size);
		int result = WideCharToMultiByte(CP_UTF8, NULL, str.c_str(), static_cast<int>(str.size()), buffer.data(), static_cast<int>(buffer.size()), NULL, NULL);
		(void)result; // unused in release builds
		assert(result == output_size);
		gsstl::string ret(buffer.begin(), buffer.end());
		return ret;
	}

	static gsstl::string platform_to_std(Platform::String^ s)
	{
		if (s == nullptr)
			return{};
		return wstring_to_utf8({ s->Begin(), s->End() });
	}

	/*
	this helper keeps track of _WindowsWebSocket instances that are still alive, so that
	we can bail out if a MessageWebSocket callback is called after the destruction
	of NativeWebSocket.

	Unregistering the callbacks would be a cleaner solution, but it turned out to be
	unreliable in a multi threaded context.
	*/
	struct LivetimeGuard
	{
		LivetimeGuard() { guard lock(mtx); aliveObjects.insert(this); }
		~LivetimeGuard() { guard lock(mtx); aliveObjects.erase(this); }
		bool isAlive() const { guard lock(mtx); return aliveObjects.count(this) > 0; }

		typedef gsstl::lock_guard<gsstl::recursive_mutex> guard;
		static gsstl::recursive_mutex mtx;
		static gsstl::set<const LivetimeGuard*> aliveObjects;
	};

	gsstl::set<const LivetimeGuard*> LivetimeGuard::aliveObjects;
	gsstl::recursive_mutex LivetimeGuard::mtx;

	class _WindowsWebSocket : public easywsclient::WebSocket
	{
		public:
			_WindowsWebSocket(const gsstl::string& url, bool binary)
			:readyState(CONNECTING)
			{
				(void)url;
				webSocket = ref new MessageWebSocket();
				webSocket->Control->MessageType = binary?Windows::Networking::Sockets::SocketMessageType::Binary : Windows::Networking::Sockets::SocketMessageType::Utf8;

				Windows::Foundation::Uri^ pUri = ref new Windows::Foundation::Uri(ref new Platform::String(utf8_to_wstring(url).c_str()));

				webSocket->Closed += ref new Windows::Foundation::TypedEventHandler<IWebSocket^, WebSocketClosedEventArgs^>([this](IWebSocket^, WebSocketClosedEventArgs^) {
					LivetimeGuard::guard lock(LivetimeGuard::mtx);
					if (!liveTimeGuard.isAlive()) return;

					PushResult({ Result::Type::Close, "Closed" });
				});

				webSocket->MessageReceived += ref new Windows::Foundation::TypedEventHandler<MessageWebSocket^, MessageWebSocketMessageReceivedEventArgs^>([this](MessageWebSocket^, MessageWebSocketMessageReceivedEventArgs^ args) {
					LivetimeGuard::guard lock(LivetimeGuard::mtx);
					if (!liveTimeGuard.isAlive()) return;

					try
					{
					Windows::Storage::Streams::DataReader^ messageReader = args->GetDataReader();

					auto array = ref new Platform::Array<unsigned char>(messageReader->UnconsumedBufferLength);
					messageReader->ReadBytes(array);

					gsstl::string str((char*)array->begin(), (char*)array->end());
					PushResult({ Result::Type::Message, str });
					}
					catch (Platform::COMException ^ex)
					{
						LivetimeGuard::guard lock2(LivetimeGuard::mtx);
						if (!liveTimeGuard.isAlive()) return;

						PushResult({ Result::Type::Error, "Error while retrieving data: " + platform_to_std(ex->Message) });
					}
				});

				auto connectOp = webSocket->ConnectAsync(pUri);
				connectOp->Completed = ref new Windows::Foundation::AsyncActionCompletedHandler([this](Windows::Foundation::IAsyncAction^ asyncAction, Windows::Foundation::AsyncStatus asyncStatus)
				{
					LivetimeGuard::guard lock(LivetimeGuard::mtx);
					if (!liveTimeGuard.isAlive()) return;

					switch (asyncStatus)
					{
						case Windows::Foundation::AsyncStatus::Completed:
						{
							PushResult({ Result::Type::Open, "opened" });
						} break;

						case Windows::Foundation::AsyncStatus::Error:
						{
							PushResult({ Result::Type::Error, "failed to connect"});
						} break;

						case Windows::Foundation::AsyncStatus::Canceled:
							break;

						case Windows::Foundation::AsyncStatus::Started:
							break;

						default:
						{
							PushResult({ Result::Type::Error, "Unknown Status" });
						} break;
					}
				});
			}

			// timeout in milliseconds
			virtual void poll(int timeout, WSErrorCallback errorCallback, void* userData)
			{
				(void)timeout;
				(void)errorCallback;
				(void)userData;
			}

			virtual void send(const gsstl::string& message)
			{
				(void)message;

				if (webSocket && readyState == OPEN)
				{
					Windows::Storage::Streams::DataWriter^ writer = ref new Windows::Storage::Streams::DataWriter(webSocket->OutputStream);
					Platform::Array<unsigned char>^ array = ref new Platform::Array<unsigned char>((unsigned char*)(message.data()), (unsigned int)message.size());
					writer->WriteBytes(array);

					using namespace concurrency;

					// Send the data as one complete message
					create_task(writer->StoreAsync()).then([this, writer](unsigned int)
					{
						// Send Completed
						writer->DetachStream();    // give the stream back to m_messageWebSocket
					}).then([this](task<void> previousTask)
					{
						try
						{
							// Try getting all exceptions from the continuation chain above this point.
							previousTask.get();
						}
						catch (Platform::COMException ^ex)
						{
							LivetimeGuard::guard lock(LivetimeGuard::mtx);
							if (!liveTimeGuard.isAlive()) return;

							PushResult({ Result::Type::Error, "Error while sending data: " + platform_to_std(ex->Message) });
						}
					});
				}
				else
				{
					PushResult({ Result::Type::Error, "Send() called on closed or currently connecting WebSocket." });
				}
			}

			virtual void sendPing()
			{
				send(" ");
			}

			virtual void close()
			{
				readyState = CLOSING;
				webSocket->Close(1000, ref new Platform::String(L"bye"));
			}

			virtual readyStateValues getReadyState() const
			{
				return readyState;
			}

			virtual ~_WindowsWebSocket()
			{

			}
		protected:
			virtual void _dispatch(WSMessageCallback message_callback, WSErrorCallback error_callback, void* data)
			{
				(void)message_callback;
				(void)error_callback;
				(void)data;

				Result entry;
				while (PopResult(entry))
				{
					switch (entry.type)
					{
						case Result::Type::Open:
							readyState = OPEN;
							break;
						case Result::Type::Close:
							readyState = CLOSED;
							break;
						case Result::Type::Error:
							if (error_callback)
							{
								error_callback(easywsclient::WSError(easywsclient::WSError::DNS_LOOKUP_FAILED, entry.data), data);
							}
							break;
						case Result::Type::Message:
							assert(message_callback);
							message_callback(entry.data, data);
							break;
					}
				}
			}

		private:
			struct Result
			{
				enum class Type
				{
					Open,
					Error,
					Message,
					Close
				};

				Result(){}

				Result(Type type, const gsstl::string& data)
				:type(type), data(data) {}

				Type type;
				gsstl::string data;
			};

			void PushResult(const Result& result)
			{
				LivetimeGuard::guard lock(LivetimeGuard::mtx);
				if (!liveTimeGuard.isAlive()) return;

				gsstl::lock_guard<gsstl::recursive_mutex> guard(resultQueueMutex);
				resultQueue.push(result);
			}

			bool PopResult(Result& result)
			{
				gsstl::lock_guard<gsstl::recursive_mutex> guard(resultQueueMutex);
				if (resultQueue.empty())
				{
					return false;
				}
				else
				{
					result = resultQueue.front();
					resultQueue.pop();
					return true;
				}
			}

			MessageWebSocket^  webSocket;

			gsstl::recursive_mutex resultQueueMutex;
			gsstl::queue<Result> resultQueue;
			readyStateValues readyState;

			// it's safest to make this the very last member, so that
			// it gets created last and destroyed first.
			LivetimeGuard liveTimeGuard;
	};
}

namespace easywsclient {
	WebSocket::pointer WebSocket::from_url(const gsstl::string& url, const gsstl::string& origin) {
		(void)url;
		(void)origin;

		//return nullptr;
		//return new _WindowsWebSocket("wss://untrusted-root.badssl.com/", false);
		//return new _WindowsWebSocket("wss://echo.websocket.org/", false);
		return new _WindowsWebSocket(url, false);
	}

	WebSocket::pointer WebSocket::from_url_binary(const gsstl::string& url, const gsstl::string& origin) {
		(void)url;
		(void)origin;

		//return nullptr;
		//return new _WindowsWebSocket("wss://untrusted-root.badssl.com/", true);
		//return new _WindowsWebSocket("wss://echo.websocket.org/", true);
		return new _WindowsWebSocket(url, true);
	}

	//WebSocket::pointer WebSocket::from_url_no_mask(const gsstl::string& url, const gsstl::string& origin) {
	//	return ::from_url(url, false, origin);
	//}
} // namespace easywsclient

#endif /* _DURANGO */
