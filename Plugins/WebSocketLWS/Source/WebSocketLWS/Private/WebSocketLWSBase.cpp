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

#include "WebSocketLWSBase.h"
#include "WebSocketLWS.h"
#include <iostream>
#include "Misc/ScopeLock.h"

#define MAX_ECHO_PAYLOAD 64*1024

extern FCriticalSection lock_websocketCtx;

UWebSocketLWSBase::UWebSocketLWSBase()
{
    mlwsContext = nullptr;
    mlws = nullptr;
}

void UWebSocketLWSBase::Setup(const FString& _uri, const TMap<FString, FString>& header, struct libwebsockets::lws_context* _mlwsContext)
{
    uri = _uri;
    mHeaderMap = header;
    mlwsContext = _mlwsContext;
}

void UWebSocketLWSBase::BeginDestroy()
{
	Super::BeginDestroy();
    FScopeLock lock(&lock_websocketCtx);

	if (mlws != nullptr)
	{
        libwebsockets::lws_set_wsi_user(mlws, NULL);
		mlws = nullptr;
	}
}

void UWebSocketLWSBase::Connect()
{
    FScopeLock lock(&lock_websocketCtx);

	if (uri.IsEmpty())
	{
		return;
	}

	if (mlwsContext == nullptr)
	{
		return;
	}

	int iUseSSL = 0;
	int iPos = uri.Find(TEXT(":"));
	if (iPos == INDEX_NONE)
	{
		//UE_LOG(WebSocketLWS, Error, TEXT("Invalid Websocket address:%s"), *uri);
		return;
	}

	FString strProtocol = uri.Left(iPos);
	if (strProtocol.ToUpper() != TEXT("WS") && strProtocol.ToUpper() != TEXT("WSS"))
	{
		//UE_LOG(WebSocketLWS, Error, TEXT("Invalid Protol:%s"), *strProtocol);
		return;
	}

	if (strProtocol.ToUpper() == TEXT("WSS"))
	{
		iUseSSL = 2;
	}

	FString strHost;
	FString strPath = TEXT("/");
	FString strNextParse = uri.Mid(iPos + 3);
	iPos = strNextParse.Find("/");
	if (iPos != INDEX_NONE)
	{
		strHost = strNextParse.Left(iPos);
		strPath = strNextParse.Mid(iPos);
	}
	else
	{
		strHost = strNextParse;
	}

	FString strAddress = strHost;
	int iPort = 80;
	iPos = strAddress.Find(":");
	if (iPos != INDEX_NONE)
	{
		strAddress = strHost.Left(iPos);
		iPort = FCString::Atoi(*strHost.Mid(iPos + 1));
	}
	else
	{
		if (iUseSSL)
		{
			iPort = 443;
		}
	}

	struct libwebsockets::lws_client_connect_info connectInfo;
	memset(&connectInfo, 0, sizeof(connectInfo));

	std::string stdAddress = TCHAR_TO_UTF8(*strAddress);
	std::string stdPath = TCHAR_TO_UTF8(*strPath);
	std::string stdHost = TCHAR_TO_UTF8(*strHost);;

	connectInfo.context = mlwsContext;
	connectInfo.address = stdAddress.c_str();
	connectInfo.port = iPort;
	connectInfo.ssl_connection = iUseSSL;
	connectInfo.path = stdPath.c_str();
	connectInfo.host = stdHost.c_str();
	connectInfo.origin = stdHost.c_str();
	connectInfo.ietf_version_or_minus_one = -1;
	connectInfo.userdata = this;

    UE_LOG(WebSocketLWS, Log, TEXT("%s:%d: uri=%s"), TEXT(__FUNCTION__), __LINE__, *uri);
	mlws = libwebsockets::lws_client_connect_via_info(&connectInfo);
	//mlws = lws_client_connect_extended(mlwsContext, TCHAR_TO_UTF8(*strAddress), iPort, iUseSSL, TCHAR_TO_UTF8(*strPath), TCHAR_TO_UTF8(*strHost), TCHAR_TO_UTF8(*strHost), NULL, -1, (void*)this);
	if (mlws == nullptr)
	{
		UE_LOG(WebSocketLWS, Error, TEXT("create client connect fail"));
		return;
	}
}

void UWebSocketLWSBase::SendText(const FString& data)
{
	if (data.Len() > MAX_ECHO_PAYLOAD)
	{
		UE_LOG(WebSocketLWS, Error, TEXT("too large package to send > MAX_ECHO_PAYLOAD:%d > %d"), data.Len(), MAX_ECHO_PAYLOAD);
		return;
	}

	if (mlws != nullptr)
	{
		mSendQueue.Add(data);
	}
	else
	{
		UE_LOG(WebSocketLWS, Error, TEXT("the socket is closed, SendText fail"));
	}
}

void UWebSocketLWSBase::ProcessWriteable()
{
	while (mSendQueue.Num() > 0)
	{
		std::string strData = TCHAR_TO_UTF8(*mSendQueue[0]);

		unsigned char buf[LWS_PRE + MAX_ECHO_PAYLOAD];
		memcpy(&buf[LWS_PRE], strData.c_str(), strData.size() );
        libwebsockets::lws_write(mlws, &buf[LWS_PRE], strData.size(), libwebsockets::LWS_WRITE_TEXT);

		mSendQueue.RemoveAt(0);
	}
}

void UWebSocketLWSBase::ProcessRead(const char* in, int len)
{
	FString strData = UTF8_TO_TCHAR(in);
	OnReceiveData.Broadcast(strData);
}


bool UWebSocketLWSBase::ProcessHeader(unsigned char** p, unsigned char* end)
{
	if (mHeaderMap.Num() == 0)
	{
		return true;
	}
	
	for (auto& it : mHeaderMap)
	{
		std::string strKey = TCHAR_TO_UTF8(*(it.Key) );
		std::string strValue = TCHAR_TO_UTF8(*(it.Value));

		strKey += ":";
		if (libwebsockets::lws_add_http_header_by_name(mlws, (const unsigned char*)strKey.c_str(), (const unsigned char*)strValue.c_str(), (int)strValue.size(), p, end))
		{
			return false;
		}
	}

	return true;
}
void UWebSocketLWSBase::Close()
{
    Cleanlws();
	OnClosed.Broadcast();
}

void UWebSocketLWSBase::Cleanlws()
{
	if (mlws != nullptr)
	{
        libwebsockets::lws_set_wsi_user(mlws, NULL);
        mlws = nullptr;
	}
}




