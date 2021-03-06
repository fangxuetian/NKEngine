#include "AsyncSocket.h"

#include <WS2tcpip.h>
#include <MSWSock.h>

#include "../NKEngineLog.h"
#include "../NKCore.h"
#include "IOCPManager.h"
#include "EventContext.h"
#include "Packet.h"
#include "SendStream.h"

using namespace NKNetwork;
using namespace std;

AsyncSocket::AsyncSocket(void)
	:_socket(INVALID_SOCKET)
	, _recv_stream(make_shared<NKCore::Buffer>(BUFFER_LENGTH_8K))
{
}

AsyncSocket::~AsyncSocket(void)
{
}

bool AsyncSocket::open(HANDLE hCompletionPort)
{
	//__GUARD__;

	_socket = WSASocket( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED );
	if (_socket == INVALID_SOCKET)
	{
		NKENGINELOG_SOCKETERROR( WSAGetLastError(), L"failed to open socket" );
		return false;
	}

	if (CreateIoCompletionPort( (HANDLE)_socket, hCompletionPort, (ULONG_PTR)IOCPManager::COMPLETION_KEY::PROCESS_EVENT, 0 ) == nullptr)
	{
		NKENGINELOG_SOCKETERROR( GetLastError(), L"failed to bind socket to completion port,socket %I64u", _socket );

		close();
		return false;
	}

	NKENGINELOG_INFO( L"success to open socket,socket %I64u", _socket );

	return true;

	//__UNGUARD__;
}

bool AsyncSocket::connect(const NKString& address, USHORT port)
{
	//__GUARD__;

	SOCKADDR_IN addr;
	memset( &addr, 0, sizeof(addr) );
	addr.sin_family = AF_INET;

	int err = ::bind( _socket, (SOCKADDR *)&addr, sizeof(addr) );
	if( err == SOCKET_ERROR )
	{
		NKENGINELOG_SOCKETERROR( WSAGetLastError(), L"failed to bind,socket %I64u", _socket );

		close();
		return false;
	}
	
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;

	if ((address[0] >= 'A' && address[0] <= 'Z') ||
		(address[0] >= 'a' && address[0] <= 'z'))
	{
		struct addrinfo *result = nullptr;
		struct addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		int ret = getaddrinfo(address.c_str(), nullptr, &hints, &result);
		if (ret != 0)
		{
			NKENGINELOG_SOCKETERROR(ret, L"faied to get host by name,socket %I64u,%S:%d", _socket, address.c_str(), port);
			return false;
		}

		memcpy( &serverAddr, result->ai_addr, sizeof(SOCKADDR_IN));
		freeaddrinfo(result);
	}
	else
	{
		inet_pton(AF_INET, address.c_str(), &serverAddr.sin_addr.s_addr);
	}

	serverAddr.sin_port = htons(port);

	EventContext *pConnectContext = new EventContext();
	memset( pConnectContext, 0, sizeof(EventContext) );
	pConnectContext->_type = EVENTCONTEXTTYPE::CONNECT;
	pConnectContext->_event_object = shared_from_this();
	
	if( IOCPManager::CONNECTEXFUNC( _socket, (SOCKADDR *)&serverAddr, sizeof(serverAddr), NULL, 0, NULL, pConnectContext ) == SOCKET_ERROR )
	{
		if( WSAGetLastError() != WSA_IO_PENDING )
		{
			NKENGINELOG_SOCKETERROR( WSAGetLastError(), L"failed to connect,socket %I64u,%S:%d", _socket, address.c_str(), port );
			return false;
		}
	}

	NKENGINELOG_INFO( L"try to connect,socket %I64u,%S:%d", _socket, address.c_str(), port );

	return true;

	//__UNGUARD__;
}

bool AsyncSocket::close(void)
{
	if( _socket == INVALID_SOCKET )
	{
		NKENGINELOG_ERROR( L"socket handle is invalid" );
		return false;
	}

	if( closesocket(_socket) == SOCKET_ERROR )
	{
		NKENGINELOG_SOCKETERROR( WSAGetLastError(), L"failed to close,socket %I64u", _socket );
		return false;
	}

	NKENGINELOG_INFO( L"closed,socket %I64u", _socket );

	_socket = INVALID_SOCKET;

	return true;
}

bool AsyncSocket::recv(void)
{
	if (_recv_stream.getRemainSize() == 0)
	{
		NKENGINELOG_ERROR( L"socket remain receive buffer size insufficient,socket %I64u", _socket );

		close();
		return false;
	}
	
	ReceiveContext* pRecvContext = new ReceiveContext(_recv_stream);
	pRecvContext->_type = EVENTCONTEXTTYPE::RECEIVE;
	pRecvContext->_event_object = shared_from_this();
	
	DWORD flags = 0;
	int recvdlen = WSARecv( _socket, pRecvContext->get(), 1, NULL, &flags, pRecvContext, NULL );
	if( recvdlen == SOCKET_ERROR )
	{
		if( WSAGetLastError() != WSA_IO_PENDING )
		{
			delete pRecvContext;
			pRecvContext = nullptr;

			NKENGINELOG_SOCKETERROR( WSAGetLastError(),  L"failed to register recv,socket %I64u", _socket );

			close();
			return false;
		}
	}

	return true;
}

//bool AsyncSocket::send(byte *pSendData, uint16_t len)
//{
//	//__GUARD__;
//
//	if (_socket == INVALID_SOCKET)
//	{
//		NKENGINELOG_ERROR(L"socket handle is invalid");
//		return false;
//	}
//
//	if( pSendData == NULL || len == 0 )
//	{
//		NKENGINELOG_ERROR( L"data is null or length is 0,socket %I64u", _socket);
//		return false;
//	}
//	
//	// memory copy가 발생하지 않기 위해서는 Send(Packet &) 함수를 사용한다.
//	BufferContext8K *pBufferContext = new BufferContext8K();
//	pBufferContext->_type = EVENTCONTEXTTYPE::SEND;
//	memcpy_s( pBufferContext->_buffer.buf, pBufferContext->_buffer.len, pSendData, len );
//	pBufferContext->_buffer.len = len;
//	//
//
//	int ret = 0;
//	DWORD sentBytes = 0;
//
//	//RCO_REF(this,RCO_REFERER_TYPE_ASYNCSOCKET);
//	
//	ret = WSASend( _socket, &pBufferContext->_buffer, 1, (LPDWORD)&sentBytes, 0, pBufferContext, NULL );
//	if( ret == SOCKET_ERROR )
//	{
//		if( WSAGetLastError() != WSA_IO_PENDING )
//		{
////			pBufferContext->Unref();
////			Unref();
//			NKENGINELOG_SOCKETERROR( WSAGetLastError(), L"failed to send,socket %I64u", _socket );
//			return false;
//		}
//	}
//
//	//__UNGUARD__;
//
//	return true;
//}

bool AsyncSocket::send(const SendStream& stream)
{
	//__GUARD__;

	if ( _socket == INVALID_SOCKET )
	{
		NKENGINELOG_ERROR( L"socket handle is invalid" );
		return false;
	}
	
	/*if ( packet.getLength() == 0 )
	{
		NKENGINELOG_ERROR( L"send stream is null, %I64u", _socket );
		return false;
	}*/

	SendContext* pSendContext = new SendContext(stream);
	pSendContext->_type = EVENTCONTEXTTYPE::SEND;
	pSendContext->_event_object = shared_from_this();
#if defined _NETWORK_SEND_DEBUG_LOG_
	NKENGINELOG_INFO( L"send,socket %I64u, buffer %p", _socket, pSendContext);
#endif
	NKENGINELOG_INFO( L"send,socket %I64u, length %u", _socket, stream.getLength() );
	
	int ret = 0;
	DWORD sentBytes = 0;
		
	ret = WSASend( _socket, pSendContext->get(), 1, (LPDWORD)&sentBytes, 0, pSendContext, NULL );
	if( ret == SOCKET_ERROR )
	{
		if( WSAGetLastError() != WSA_IO_PENDING )
		{
			delete pSendContext;
			pSendContext = nullptr;

			NKENGINELOG_SOCKETERROR( WSAGetLastError(), L"failed to send,socket %I64u", _socket );

			return false;
		}
	}

	//__UNGUARD__;

	return true;
}

bool AsyncSocket::disconnect(void)
{
	if( _socket == INVALID_SOCKET )
	{
		NKENGINELOG_ERROR( L"socket handle is invalid" );
		return false;
	}

	if( shutdown(_socket,SD_SEND) == SOCKET_ERROR )
	{
		NKENGINELOG_SOCKETERROR( WSAGetLastError(), L"failed to disconnect,socket %I64u", _socket );
		close();
		return false;
	}

	NKENGINELOG_INFO( L"disconnect an async socket,socket %I64u", _socket );
	
	return true;
}

bool AsyncSocket::onProcess(EventContext& event_context, uint32_t transferred)
{
//	__GUARD__;

	_ASSERT( event_context._type == EVENTCONTEXTTYPE::RECEIVE || event_context._type == EVENTCONTEXTTYPE::SEND || event_context._type == EVENTCONTEXTTYPE::CONNECT );

	switch( event_context._type )
	{
	case EVENTCONTEXTTYPE::RECEIVE:
		{
			if( transferred == 0 )
			{
				close();
				onClosed();
			}
			else
			{
				if(_recv_stream.update( transferred ) == false )
				{
					NKENGINELOG_ERROR( L"receive buffer is insufficient,socket %I64u,length %d", _socket, transferred );
					NKENGINELOG_ERROR( L"FORCE CLOSE,socket %I64u", _socket );
					disconnect();
					break;
				}

				while(_recv_stream.getLength() >= sizeof(PROTOCOLHEAD::size_type))
				{
					PROTOCOLHEAD::size_type length = _recv_stream.peek<PROTOCOLHEAD::size_type>();

					NKENGINELOG_INFO( L"receive packet,socket %I64u,length %d/%d,transferred %d", _socket, _recv_stream.getLength(), length, transferred );
				
					// packet parsing
					if( length == 0 || length > BUFFER_LENGTH_8K)
					{
						NKENGINELOG_ERROR( L"wrong size packet,socket %I64u,length %d/%d,transferred %d", _socket, _recv_stream.getLength(), length, transferred );
						NKENGINELOG_ERROR( L"FORCE CLOSE,socket %I64u", _socket );
						disconnect();
						return false;
					}
					// @nolimitk packet has completed.
					else if( length <= _recv_stream.getLength() )
					{
						NKENGINELOG_INFO(L"packet completed,socket %I64u,length %d/%d", _socket, length, transferred);

						Packet packet;
						if (packet.read(_recv_stream) == false)
						{
							NKENGINELOG_INFO(L"packet read failed,socket %I64u,length %d, %d/%d", _socket, _recv_stream.getLength(), length, transferred);
							_ASSERT(0);
							disconnect();
							return false;
						}

						onReceived(packet);

						if(_recv_stream.getLength() != 0 )
						{
							NKENGINELOG_INFO( L"packet remained,socket %I64u,length %d, %d/%d", _socket, _recv_stream.getLength(), length, transferred );
						}
					}
					else // if( length > _recv_packet.GetLength() )
					{
						NKENGINELOG_INFO( L"incomplete packet,socket %I64u,length %d/%d", _socket, length, transferred );
						break;
					}
				}
				_recv_stream.moveRead();

				recv();
			}
		}
		break;
	case EVENTCONTEXTTYPE::SEND:
		{
			NKENGINELOG_INFO( L"send completed,socket %I64u,length %d", _socket, transferred );

			onSent();
		}
		break;
	case EVENTCONTEXTTYPE::CONNECT:
		{
			setsockopt( _socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0 );

			onConnected();

			recv();
			
			NKENGINELOG_INFO( L"connect completed,socket %I64u,length %d", _socket, transferred );
		}
		break;
	}
	
	return true;

	//__UNGUARD__;
}

bool AsyncSocket::onProcessFailed(EventContext& event_context, uint32_t transferred)
{
	//__GUARD__;

	_ASSERT( event_context._type == EVENTCONTEXTTYPE::RECEIVE || event_context._type == EVENTCONTEXTTYPE::SEND || event_context._type == EVENTCONTEXTTYPE::CONNECT );

	switch( event_context._type )
	{
	case EVENTCONTEXTTYPE::RECEIVE:
		{
			uint32_t errorCode = GetLastError();
			switch( errorCode )
			{
				// peer가 정상적으로 종료되지 않음 [2014/11/17/ nolimitk]
			case ERROR_NETNAME_DELETED:
				break;
			default:
				NKENGINELOG_SOCKETERROR( GetLastError(), L"connection closed,socket %I64u", _socket );
				break;
			}

			// shutdown실패로 인해 이미 close 되어 있을수도 있다.
			if( INVALID_SOCKET != _socket )
			{
				close();
			}
			onClosed();
		}
		break;
	case EVENTCONTEXTTYPE::SEND:
		{
			NKENGINELOG_SOCKETERROR( GetLastError(), L"send failed,socket %I64u,length %u", _socket, transferred );
				
			close();
		}
		break;
	case EVENTCONTEXTTYPE::CONNECT:
		{
			uint32_t errorCode = GetLastError();
			switch( errorCode )
			{
				// connect to specified address (peer ip), but port does not listen. [2014/11/17/ nolimitk]
			case ERROR_CONNECTION_REFUSED:
				break;
			default:
				NKENGINELOG_SOCKETERROR( errorCode, L"connect failed,socket %I64u,length %u", _socket, transferred );
				break;
			}

			close();
			onConnectFailed();
		}
		break;
	default:
		{
			NKENGINELOG_SOCKETERROR( GetLastError(), L"event failed,socket %I64u,type %d", _socket, event_context._type);
		}
		break;
	}
	
	return true;

	//__UNGUARD__;
}

bool AsyncSocket::onClosed(void)
{
	return true;
}

bool AsyncSocket::onConnected(void)
{
	return true;
}

bool AsyncSocket::onConnectFailed(void)
{
	return true;
}

bool AsyncSocket::onReceived(const Packet& packet)
{
	return true;
}

bool AsyncSocket::onSent(void)
{
	return true;
}