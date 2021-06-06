/*
* uewebsocket - unreal engine 4 websocket plugin
*
* Copyright (C) 2017 feiwu <feixuwu@outlook.com>
* Copyright (C) 2021 Maksym Veremeyenko <verem@m1.tv>
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

#pragma once

#include "Components/ActorComponent.h"
#include "UObject/NoExportTypes.h"
#include "Delegates/DelegateCombinations.h"
namespace libwebsockets {
#include "libwebsockets.h"
}
#include "WebSocketLWSBase.generated.h"


DEFINE_LOG_CATEGORY_STATIC(WebSocketLWS, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWebSocketLWSConnectError, const FString&, error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWebSocketLWSClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWebSocketLWSConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWebSocketLWSRecieve, const FString&, data);

struct lws_context;
struct lws;

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class WEBSOCKETLWS_API UWebSocketLWSBase:public UObject
{
	GENERATED_BODY()
public:

    UWebSocketLWSBase();

    void Setup(const FString& uri, const TMap<FString, FString>& header, struct libwebsockets::lws_context* mlwsContext);

	virtual void BeginDestroy() override;
	
	UFUNCTION(BlueprintCallable, Category = WebSocketLWS)
	void SendText(const FString& data);

	UFUNCTION(BlueprintCallable, Category = WebSocketLWS)
	void Close();

    UFUNCTION(BlueprintCallable, Category = WebSocketLWS)
	void Connect();

	UPROPERTY(BlueprintAssignable, Category = WebSocketLWS)
	FWebSocketLWSConnectError OnConnectError;

	UPROPERTY(BlueprintAssignable, Category = WebSocketLWS)
	FWebSocketLWSClosed OnClosed;

	UPROPERTY(BlueprintAssignable, Category = WebSocketLWS)
	FWebSocketLWSConnected OnConnectComplete;

	UPROPERTY(BlueprintAssignable, Category = WebSocketLWS)
	FWebSocketLWSRecieve OnReceiveData;

	void Cleanlws();
	void ProcessWriteable();
	void ProcessRead(const char* in, int len);
	bool ProcessHeader(unsigned char** p, unsigned char* end);

	struct libwebsockets::lws_context* mlwsContext;
	struct libwebsockets::lws* mlws;
	TArray<FString> mSendQueue;
    FString uri;
    TMap<FString, FString> mHeaderMap;
};
