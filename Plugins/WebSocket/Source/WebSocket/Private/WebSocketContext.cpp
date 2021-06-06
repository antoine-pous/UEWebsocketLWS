/*
* uewebsocket - unreal engine 4 websocket plugin
*
* Copyright (C) 2017 feiwu <feixuwu@outlook.com>
*
*  This library is free software; you can redistribute it and/or
*  modify it under the terms of the GNU Lesser General Public
*  License as published by the Free Software Foundation:
*  version 2.1 of the License.
*
*  This library is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  Lesser General Public License for more details.
*
*  You should have received a copy of the GNU Lesser General Public
*  License along with this library; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
*  MA  02110-1301  USA
*/

#include "WebSocketContext.h"
#include "WebSocket.h"
#include "UObjectGlobals.h"
#include "WebSocketBase.h"
#include "Misc/ScopeLock.h"

#define MAX_PAYLOAD	64*1024

extern FCriticalSection lock_websocketCtx;
extern TSharedPtr<UWebSocketContext> s_websocketCtx;

static struct libwebsockets::lws_protocols protocols[] = {
	/* first protocol must always be HTTP handler */

	{
		"",		/* name - can be overridden with -e */
		UWebSocketContext::callback_echo,
		0,
		MAX_PAYLOAD,
	},
	{
		NULL, NULL, 0		/* End of list */
	}
};

static const struct libwebsockets::lws_extension exts[] = {
	{
		"permessage-deflate",
        libwebsockets::lws_extension_callback_pm_deflate,
		"permessage-deflate; client_no_context_takeover"
	},
	{
		"deflate-frame",
        libwebsockets::lws_extension_callback_pm_deflate,
		"deflate_frame"
	},
	{ NULL, NULL, NULL /* terminator */ }
};

int UWebSocketContext::callback_echo(struct libwebsockets::lws *wsi, enum libwebsockets::lws_callback_reasons reason, void *user, void *in, size_t len)
{
	void* pUser = libwebsockets::lws_wsi_user(wsi);
	UWebSocketBase* pWebSocketBase = (UWebSocketBase*)pUser;

    FScopeLock lock(&lock_websocketCtx);

    if(libwebsockets::LWS_CALLBACK_CLIENT_WRITEABLE != reason && libwebsockets::LWS_CALLBACK_CLIENT_RECEIVE != reason)
        UE_LOG(WebSocket, Log, TEXT("%s:%d: reason=%d, url=%s"), TEXT(__FUNCTION__), __LINE__,
            reason, !pWebSocketBase ? TEXT("") : *pWebSocketBase->prev_uri);

	switch (reason)
	{
    case libwebsockets::LWS_CALLBACK_WSI_DESTROY:
        if (!pWebSocketBase) return -1;
        pWebSocketBase->Cleanlws();
        break;

    case libwebsockets::LWS_CALLBACK_CLIENT_CLOSED:
    case libwebsockets::LWS_CALLBACK_CLOSED_CLIENT_HTTP:
		if (!pWebSocketBase) return -1;
		pWebSocketBase->OnClosed.Broadcast();
		break;

	case libwebsockets::LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
	{
        FString strError = UTF8_TO_TCHAR(in);
        UE_LOG(WebSocket, Error, TEXT("libwebsocket connect error:%s"), *strError);
        if (!pWebSocketBase) return -1;
		pWebSocketBase->OnConnectError.Broadcast(strError);
	}
		break;

	case libwebsockets::LWS_CALLBACK_CLIENT_ESTABLISHED:
		if (!pWebSocketBase) return -1;
		pWebSocketBase->OnConnectComplete.Broadcast();
		break;

	case libwebsockets::LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
	{
		if (!pWebSocketBase) return -1;

		unsigned char **p = (unsigned char **)in, *end = (*p) + len;
		if (!pWebSocketBase->ProcessHeader(p, end))
		{
			return -1;
		}
	}
		break;

	case libwebsockets::LWS_CALLBACK_CLIENT_RECEIVE:
		if (!pWebSocketBase) return -1;
		pWebSocketBase->ProcessRead((const char*)in, (int)len);
		break;

	case libwebsockets::LWS_CALLBACK_CLIENT_WRITEABLE:
		if (!pWebSocketBase) return -1;
		pWebSocketBase->ProcessWriteable();
		break;

	default:
        break;
	}

	return 0;
}

UWebSocketContext::UWebSocketContext()
{
	mlwsContext = nullptr;
}

void UWebSocketContext::CreateCtx()
{
    FScopeLock lock(&lock_websocketCtx);

    if (mlwsContext != nullptr)
    {
        UE_LOG(WebSocket, Error, TEXT("%s:%d: context already created"), TEXT(__FUNCTION__), __LINE__);
        return;
    }

	struct libwebsockets::lws_context_creation_info info;
	memset(&info, 0, sizeof info);

	info.protocols = protocols;
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;

	info.port = -1;
	info.gid = -1;
	info.uid = -1;
	info.extensions = exts;
	info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;
	info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

	mlwsContext = libwebsockets::lws_create_context(&info);
    if (mlwsContext == nullptr)
	{
		UE_LOG(WebSocket, Error, TEXT("libwebsocket Init fail"));
	}
}

void UWebSocketContext::Tick(float DeltaTime)
{
    FScopeLock lock(&lock_websocketCtx);

    if (mlwsContext != nullptr)
    {
        libwebsockets::lws_callback_on_writable_all_protocol(mlwsContext, &protocols[0]);
        libwebsockets::lws_service(mlwsContext, -1);
	}
}

bool UWebSocketContext::IsTickable() const
{
	return true;
}

TStatId UWebSocketContext::GetStatId() const
{
	return TStatId();
}

UWebSocketBase* UWebSocketContext::Connect(const FString& uri, const TMap<FString, FString>& header)
{
	if (mlwsContext == nullptr)
	{
		return nullptr;
	}

	UWebSocketBase* pNewSocketBase = NewObject<UWebSocketBase>();
	pNewSocketBase->mlwsContext = mlwsContext;

	pNewSocketBase->Connect(uri, header);

	return pNewSocketBase;
}

UWebSocketBase* UWebSocketContext::Connect(const FString& uri)
{
	return Connect(uri, TMap<FString, FString>() );
}

