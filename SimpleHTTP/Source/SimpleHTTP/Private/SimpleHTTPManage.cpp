// Copyright (C) RenZhai.2020.All Rights Reserved.
#include "SimpleHTTPManage.h"
#include "HTTP/SimpleHttpActionMultpleRequest.h"
#include "HTTP/SimpleHttpActionSingleRequest.h"
#include "Core/SimpleHttpMacro.h"
#include "Misc/FileHelper.h"
#include "SimpleHTTPLog.h"
#include "HttpModule.h"
#include "HttpManager.h"

#if PLATFORM_WINDOWS
#pragma optimize("",off) 
#endif

FSimpleHttpManage* FSimpleHttpManage::Instance = NULL;

TSharedPtr<FSimpleHttpActionRequest> GetHttpActionRequest(EHTTPRequestType RequestType)
{
	TSharedPtr<FSimpleHttpActionRequest> HttpObject = nullptr;
	switch (RequestType)
	{
		case EHTTPRequestType::SINGLE:
		{
			HttpObject = MakeShareable(new FSimpleHttpActionSingleRequest());
			UE_LOG(LogSimpleHTTP, Log, TEXT("Action to create a single HTTP request"));
			break;
		}
		case EHTTPRequestType::MULTPLE:
		{
			HttpObject = MakeShareable(new FSimpleHttpActionMultpleRequest());
			UE_LOG(LogSimpleHTTP, Log, TEXT("Action to create a multple HTTP request"));
			break;
		}
	}

	return HttpObject;
}

void FSimpleHttpManage::Tick(float DeltaTime)
{
	FScopeLock ScopeLock(&Instance->Mutex);

	if (!HTTP.bPause)
	{
		FHttpModule::Get().GetHttpManager().Tick(DeltaTime);
	}
	
	TArray<FName> RemoveRequest;
	for (auto &Tmp : HTTP.HTTPMap)
	{
		if (Tmp.Value->IsRequestComplete())
		{
			RemoveRequest.Add(Tmp.Key);
		}
	}

	for (auto &Tmp : RemoveRequest)
	{
		GetHTTP().HTTPMap.Remove(Tmp);

		UE_LOG(LogSimpleHTTP, Log, TEXT("Remove request %s from tick"), *Tmp.ToString());
	}
}

bool FSimpleHttpManage::IsTickableInEditor() const
{
	return true;
}

TStatId FSimpleHttpManage::GetStatId() const
{
	return TStatId();
}

FSimpleHttpManage * FSimpleHttpManage::Get()
{
	if (Instance == nullptr)
	{
		Instance = new FSimpleHttpManage();

		UE_LOG(LogSimpleHTTP, Log, TEXT("Get HTTP management"));
	}

	return Instance;
}

void FSimpleHttpManage::Destroy()
{
	if (Instance != nullptr)
	{
		FScopeLock ScopeLock(&Instance->Mutex);
		delete Instance;		

		UE_LOG(LogSimpleHTTP, Log, TEXT("delete HTTP management"));
	}

	Instance = nullptr;
}

FSimpleHTTPHandle FSimpleHttpManage::FHTTP::RegisteredHttpRequest(
	EHTTPRequestType RequestType ,
	FSimpleHttpSingleRequestCompleteDelegate SimpleHttpRequestCompleteDelegate /*= FSimpleHttpRequestCompleteDelegate()*/,
	FSimpleHttpSingleRequestProgressDelegate SimpleHttpRequestProgressDelegate /*= FSimpleHttpRequestProgressDelegate()*/, 
	FSimpleHttpSingleRequestHeaderReceivedDelegate SimpleHttpRequestHeaderReceivedDelegate /*= FSimpleHttpRequestHeaderReceivedDelegate()*/,
	FAllRequestCompleteDelegate AllRequestCompleteDelegate /*= FAllRequestCompleteDelegate()*/)
{
	FScopeLock ScopeLock(&Instance->Mutex);

	UE_LOG(LogSimpleHTTP, Log, TEXT("Start registering single BP agent."));

	TSharedPtr<FSimpleHttpActionRequest> HttpObject = GetHttpActionRequest(RequestType);
	
	HttpObject->SimpleHttpRequestCompleteDelegate = SimpleHttpRequestCompleteDelegate;
	HttpObject->SimpleHttpRequestHeaderReceivedDelegate = SimpleHttpRequestHeaderReceivedDelegate;
	HttpObject->SimpleHttpRequestProgressDelegate = SimpleHttpRequestProgressDelegate;
	HttpObject->AllRequestCompleteDelegate = AllRequestCompleteDelegate;

	FSimpleHTTPHandle Key = *FGuid::NewGuid().ToString();
	HTTPMap.Add(Key,HttpObject);

	return Key;
}

FSimpleHTTPHandle FSimpleHttpManage::FHTTP::RegisteredHttpRequest(
	EHTTPRequestType RequestType /*= EHTTPRequestType::SINGLE*/, 
	FSimpleSingleCompleteDelegate SimpleHttpRequestCompleteDelegate /*= nullptr*/, 
	FSimpleSingleRequestProgressDelegate SimpleHttpRequestProgressDelegate /*= nullptr*/,
	FSimpleSingleRequestHeaderReceivedDelegate SimpleHttpRequestHeaderReceivedDelegate /*= nullptr*/,
	FSimpleDelegate AllRequestCompleteDelegate /*= nullptr*/)
{
	FScopeLock ScopeLock(&Instance->Mutex);

	UE_LOG(LogSimpleHTTP, Log, TEXT("Start registering single C++ agent."));

	TSharedPtr<FSimpleHttpActionRequest> HttpObject = GetHttpActionRequest(RequestType);

	HttpObject->SimpleCompleteDelegate = SimpleHttpRequestCompleteDelegate;
	HttpObject->SimpleSingleRequestHeaderReceivedDelegate = SimpleHttpRequestHeaderReceivedDelegate;
	HttpObject->SimpleSingleRequestProgressDelegate = SimpleHttpRequestProgressDelegate;
	HttpObject->AllTasksCompletedDelegate = AllRequestCompleteDelegate;

	FSimpleHTTPHandle Key = *FGuid::NewGuid().ToString();
	HTTPMap.Add(Key, HttpObject);

	return Key;
}

TWeakPtr<FSimpleHttpActionRequest> FSimpleHttpManage::FHTTP::Find(const FSimpleHTTPHandle &Handle)
{
	FScopeLock ScopeLock(&Instance->Mutex);

	TWeakPtr<FSimpleHttpActionRequest> Object = nullptr;
	for (auto &Tmp : Instance->GetHTTP().HTTPMap)
	{
		if (Tmp.Key == Handle)
		{
			Object = Tmp.Value;

			UE_LOG(LogSimpleHTTP, Log, TEXT("Http Find at %s"),*(Tmp.Key.ToString()));
			break;
		}
	}

	return Object;
}

bool FSimpleHttpManage::FHTTP::GetObjectToMemory(const FSimpleHTTPHandle &Handle, const FString &URL, bool bSynchronous)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		Object.Pin()->SetAsynchronousState(!bSynchronous);
		return Object.Pin()->GetObject(URL);
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
	}

	return false;
}

bool FSimpleHttpManage::FHTTP::GetObjectToMemory(const FSimpleHTTPBPResponseDelegate &BPResponseDelegate, const FString &URL,bool bSynchronous)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::SINGLE);

	return GetObjectToMemory(Handle, URL, bSynchronous);
}

bool FSimpleHttpManage::FHTTP::GetObjectToMemory(const FSimpleHTTPResponseDelegate &BPResponseDelegate, const FString &URL, bool bSynchronous)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::SINGLE);

	return GetObjectToMemory(Handle, URL, bSynchronous);
}

void FSimpleHttpManage::FHTTP::GetObjectsToMemory(const FSimpleHTTPHandle &Handle, const TArray<FString> &URL, bool bSynchronous)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		Object.Pin()->SetAsynchronousState(!bSynchronous);
		Object.Pin()->GetObjects(URL);
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
	}
}

void FSimpleHttpManage::FHTTP::GetObjectsToMemory(const FSimpleHTTPBPResponseDelegate &BPResponseDelegate, const TArray<FString> &URL, bool bSynchronous)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::MULTPLE);

	GetObjectsToMemory(Handle, URL, bSynchronous);
}

void FSimpleHttpManage::FHTTP::GetObjectsToMemory(const FSimpleHTTPResponseDelegate &BPResponseDelegate, const TArray<FString> &URL, bool bSynchronous)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::MULTPLE);

	GetObjectsToMemory(Handle, URL, bSynchronous);
}

bool FSimpleHttpManage::FHTTP::GetObjectToLocal(const FSimpleHTTPHandle &Handle, const FString &URL, const FString &SavePaths)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		return Object.Pin()->GetObject(URL, SavePaths);
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
	}

	return false;
}

bool FSimpleHttpManage::FHTTP::GetObjectToLocal(const FSimpleHTTPResponseDelegate &BPResponseDelegate, const FString &URL, const FString &SavePaths)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::SINGLE);

	return GetObjectToLocal(Handle, URL, SavePaths);
}

bool FSimpleHttpManage::FHTTP::GetObjectToLocal(const FSimpleHTTPBPResponseDelegate &BPResponseDelegate, const FString &URL, const FString &SavePaths)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::SINGLE);

	return GetObjectToLocal(Handle, URL, SavePaths);
}

void FSimpleHttpManage::FHTTP::GetObjectsToLocal(const FSimpleHTTPHandle &Handle, const TArray<FString> &URL, const FString &SavePaths)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		Object.Pin()->GetObjects(URL, SavePaths);
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
	}
}

void FSimpleHttpManage::FHTTP::GetObjectsToLocal(const FSimpleHTTPResponseDelegate &BPResponseDelegate, const TArray<FString> &URL, const FString &SavePaths)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::MULTPLE);

	GetObjectsToLocal(Handle, URL, SavePaths);
}

void FSimpleHttpManage::FHTTP::GetObjectsToLocal(const FSimpleHTTPBPResponseDelegate &BPResponseDelegate, const TArray<FString> &URL, const FString &SavePaths)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::MULTPLE);

	GetObjectsToLocal(Handle, URL, SavePaths);
}

bool FSimpleHttpManage::FHTTP::PutObjectFromBuffer(const FSimpleHTTPHandle &Handle, const FString &URL, const TArray<uint8> &Data)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		return Object.Pin()->PutObject(URL, Data);
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
	}

	return false;
}

bool FSimpleHttpManage::FHTTP::PutObjectFromString(const FSimpleHTTPHandle& Handle, const FString& URL, const FString& Buffer, bool bSynchronouse)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		Object.Pin()->SetAsynchronousState(!bSynchronouse);
		return Object.Pin()->PutObjectByString(URL, Buffer);
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
	}

	return false;
}

bool FSimpleHttpManage::FHTTP::PutObjectFromBuffer(const FSimpleHTTPBPResponseDelegate &BPResponseDelegate, const FString &URL, const TArray<uint8> &Buffer)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::SINGLE);

	return PutObjectFromBuffer(Handle, URL, Buffer);
}

bool FSimpleHttpManage::FHTTP::PutObjectFromString(const FSimpleHTTPBPResponseDelegate& BPResponseDelegate, const FString& URL, const FString& InBuffer, bool bSynchronouse)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::SINGLE);

	return PutObjectFromString(Handle, URL, InBuffer, bSynchronouse);
}

bool FSimpleHttpManage::FHTTP::PutObjectFromBuffer(const FSimpleHTTPResponseDelegate &BPResponseDelegate, const FString &URL, const TArray<uint8> &Buffer)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::SINGLE);

	return PutObjectFromBuffer(Handle, URL, Buffer);
}

bool FSimpleHttpManage::FHTTP::PutObjectFromString(const FSimpleHTTPResponseDelegate& BPResponseDelegate, const FString& URL, const FString& InBuffer,bool bSynchronouse)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::SINGLE);

	return PutObjectFromString(Handle, URL, InBuffer, bSynchronouse);
}

bool FSimpleHttpManage::FHTTP::PutObjectFromStream(const FSimpleHTTPHandle &Handle, const FString &URL, TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		return Object.Pin()->PutObject(URL, Stream);
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
	}

	return false;
}

bool FSimpleHttpManage::FHTTP::PutObjectFromStream(const FSimpleHTTPBPResponseDelegate &BPResponseDelegate, const FString &URL, TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::SINGLE);

	return PutObjectFromStream(Handle, URL, Stream);
}

bool FSimpleHttpManage::FHTTP::PutObjectFromStream(const FSimpleHTTPResponseDelegate &BPResponseDelegate, const FString &URL, TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::SINGLE);

	return PutObjectFromStream(Handle, URL, Stream);
}

bool FSimpleHttpManage::FHTTP::PutObjectFromLocal(const FSimpleHTTPHandle &Handle, const FString &URL, const FString &LocalPaths, bool bSynchronouse)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		Object.Pin()->SetAsynchronousState(!bSynchronouse);
		return Object.Pin()->PutObject(URL, LocalPaths);
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
	}

	return false;
}

bool FSimpleHttpManage::FHTTP::PostObjectFromLocal(const FSimpleHTTPHandle& Handle, const FString& URL, const FString& LocalPaths, bool bSynchronouse)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		Object.Pin()->SetAsynchronousState(!bSynchronouse);
		return Object.Pin()->PostObject(URL, LocalPaths);
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
	}

	return false;
}

bool FSimpleHttpManage::FHTTP::PutObjectFromLocal(const FSimpleHTTPBPResponseDelegate &BPResponseDelegate, const FString &URL, const FString &LocalPaths,bool bSynchronouse)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::SINGLE);

	return PutObjectFromLocal(Handle, URL, LocalPaths, bSynchronouse);
}

bool FSimpleHttpManage::FHTTP::PutObjectFromLocal(const FSimpleHTTPResponseDelegate &BPResponseDelegate, const FString &URL, const FString &LocalPaths, bool bSynchronouse)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::SINGLE);

	return PutObjectFromLocal(Handle, URL, LocalPaths, bSynchronouse);
}

bool FSimpleHttpManage::FHTTP::PostObjectFromLocal(const FSimpleHTTPResponseDelegate& BPResponseDelegate, const FString& URL, const FString& LocalPaths, bool bSynchronouse)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::SINGLE);

	return PostObjectFromLocal(Handle, URL, LocalPaths, bSynchronouse);
}

bool FSimpleHttpManage::FHTTP::PutObjectsFromLocal(const FSimpleHTTPHandle &Handle, const FString &URL, const FString &LocalPaths, bool bSynchronouse)
{
	return PutObjectFromLocal(Handle, URL, LocalPaths, bSynchronouse);
}

bool FSimpleHttpManage::FHTTP::PutObjectsFromLocal(const FSimpleHTTPBPResponseDelegate &BPResponseDelegate, const FString &URL, const FString &LocalPaths, bool bSynchronouse)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::MULTPLE);

	return PutObjectsFromLocal(Handle, URL, LocalPaths, bSynchronouse);
}

bool FSimpleHttpManage::FHTTP::PutObjectsFromLocal(const FSimpleHTTPResponseDelegate &BPResponseDelegate, const FString &URL, const FString &LocalPaths, bool bSynchronouse)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::MULTPLE);

	return PutObjectsFromLocal(Handle, URL, LocalPaths, bSynchronouse);
}

bool FSimpleHttpManage::FHTTP::PostObjectsFromLocal(const FSimpleHTTPResponseDelegate& BPResponseDelegate, const FString& URL, const FString& LocalPaths, bool bSynchronouse)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::MULTPLE);

	return PostObjectFromLocal(Handle, URL, LocalPaths, bSynchronouse);
}

bool FSimpleHttpManage::FHTTP::DeleteObject(const FSimpleHTTPHandle &Handle, const FString &URL)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		return Object.Pin()->DeleteObject(URL);
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
	}

	return false;
}

bool FSimpleHttpManage::FHTTP::DeleteObject(const FSimpleHTTPBPResponseDelegate &BPResponseDelegate, const FString &URL)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::SINGLE);

	return DeleteObject(Handle, URL);
}

bool FSimpleHttpManage::FHTTP::DeleteObject(const FSimpleHTTPResponseDelegate &BPResponseDelegate, const FString &URL)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::SINGLE);

	return DeleteObject(Handle, URL);
}

void FSimpleHttpManage::FHTTP::DeleteObjects(const FSimpleHTTPHandle &Handle, const TArray<FString> &URL)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		Object.Pin()->DeleteObjects(URL);
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
	}
}

void FSimpleHttpManage::FHTTP::DeleteObjects(const FSimpleHTTPBPResponseDelegate &BPResponseDelegate, const TArray<FString> &URL)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::MULTPLE);

	DeleteObjects(Handle, URL);
}

void FSimpleHttpManage::FHTTP::DeleteObjects(const FSimpleHTTPResponseDelegate &BPResponseDelegate, const TArray<FString> &URL)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::MULTPLE);

	DeleteObjects(Handle, URL);
}

bool FSimpleHttpManage::FHTTP::PostRequest(const TCHAR *InURL, const TCHAR *InParam, const FSimpleHTTPBPResponseDelegate &BPResponseDelegate, bool bSynchronouse)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::SINGLE);

	return PostRequest(Handle,InURL + FString(TEXT("?")) + InParam);
}

bool FSimpleHttpManage::FHTTP::PostRequest(const FSimpleHTTPHandle &Handle, const FString &URL, bool bSynchronouse)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);

	if (Object.IsValid())
	{
		Object.Pin()->SetAsynchronousState(!bSynchronouse);
		Object.Pin()->PostObject(URL);

		return true;
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
		return false;
	}
}

bool FSimpleHttpManage::FHTTP::PostRequest(const TCHAR *InURL, const TCHAR *InParam,const FSimpleHTTPResponseDelegate &BPResponseDelegate, bool bSynchronouse)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::SINGLE);

	return PostRequest(Handle, InURL + FString(TEXT("?")) + InParam, bSynchronouse);
}

TArray<uint8> NULLArrayUint8;
TArray<uint8> & USimpleHttpContent::GetContent()
{
	if (Content)
	{
		return *Content;
	}

	return NULLArrayUint8;
}

bool USimpleHttpContent::Save(const FString &LocalPath)
{
	return FFileHelper::SaveArrayToFile(*Content, *LocalPath);
}

FSimpleHttpManage::FHTTP::FHTTP()
	:bPause(false)
{
}

void FSimpleHttpManage::FHTTP::Suspend()
{
	bPause = true;
}

void FSimpleHttpManage::FHTTP::Awaken()
{
	bPause = false;
}

bool FSimpleHttpManage::FHTTP::Suspend(const FSimpleHTTPHandle& Handle)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		return Object.Pin()->Suspend();
	}

	return false;
}

bool FSimpleHttpManage::FHTTP::Cancel()
{
	bool bCancel = true;
	for (auto& Tmp : Instance->GetHTTP().HTTPMap)
	{
		if (!Tmp.Value->Cancel())
		{
			bCancel = false;
		}
	}

	return bCancel;
}

bool FSimpleHttpManage::FHTTP::Cancel(const FSimpleHTTPHandle& Handle)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);
	if (Object.IsValid())
	{
		return Object.Pin()->Cancel();
	}

	return false;
}

FSimpleHTTPHandle FSimpleHttpManage::FHTTP::GetHandleByLastExecutionRequest()
{
	return TemporaryStorageHandle;
}

bool FSimpleHttpManage::FHTTP::RequestByConentString(
	ESimpleHTTPVerbType InType,
	const FString& InURL, 
	const TMap<FString, FString>& InHeadMeta, 
	const FString& InContent,
	const FSimpleHTTPBPResponseDelegate& BPResponseDelegate)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::SINGLE);

	return RequestByConentString(Handle, InType, InURL, InHeadMeta, InContent);
}

bool FSimpleHttpManage::FHTTP::RequestByConentByte(ESimpleHTTPVerbType InType, const FString& InURL, const TMap<FString, FString>& InHeadMeta, const TArray<uint8>& InBytes, const FSimpleHTTPBPResponseDelegate& BPResponseDelegate)
{
	SIMPLE_HTTP_REGISTERED_REQUEST_BP(EHTTPRequestType::SINGLE);

	return RequestByConentByte(Handle,InType,InURL, InHeadMeta, InBytes);
}

bool FSimpleHttpManage::FHTTP::RequestByConentString(ESimpleHTTPVerbType InType, const FString& InURL, const TMap<FString, FString>& InHeadMeta, const FString& InContent, const FSimpleHTTPResponseDelegate& BPResponseDelegate)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::SINGLE);

	return RequestByConentString(Handle, InType, InURL, InHeadMeta, InContent);
}

bool FSimpleHttpManage::FHTTP::RequestByConentByte(ESimpleHTTPVerbType InType, const FString& InURL, const TMap<FString, FString>& InHeadMeta, const TArray<uint8>& InBytes, const FSimpleHTTPResponseDelegate& BPResponseDelegate)
{
	SIMPLE_HTTP_REGISTERED_REQUEST(EHTTPRequestType::SINGLE);

	return RequestByConentByte(Handle, InType, InURL, InHeadMeta, InBytes);
}

bool FSimpleHttpManage::FHTTP::RequestByConentString(
	FSimpleHTTPHandle Handle, 
	ESimpleHTTPVerbType InType, 
	const FString& InURL,
	const TMap<FString, FString>& InHeadMeta,
	const FString& InContent)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);

	if (Object.IsValid())
	{
		Object.Pin()->SendRequest(InType, InURL, InHeadMeta, InContent);

		return true;
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
		return false;
	}
}

bool FSimpleHttpManage::FHTTP::RequestByConentByte(FSimpleHTTPHandle Handle, ESimpleHTTPVerbType InType, const FString& InURL, const TMap<FString, FString>& InHeadMeta, const TArray<uint8>& InBytes)
{
	TWeakPtr<FSimpleHttpActionRequest> Object = Find(Handle);

	if (Object.IsValid())
	{
		Object.Pin()->SendRequest(InType, InURL, InHeadMeta, InBytes);

		return true;
	}
	else
	{
		UE_LOG(LogSimpleHTTP, Warning, TEXT("The handle was not found [%s]"), *(Handle.ToString()));
		return false;
	}
}

#if PLATFORM_WINDOWS
#pragma optimize("",on) 
#endif