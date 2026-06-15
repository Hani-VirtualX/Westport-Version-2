// Copyright Epic Games, Inc. All Rights Reserved.

#include "ManualVRPluginBPLibrary.h"
#include "ManualVRPlugin.h"
#include "Kismet/KismetStringLibrary.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "JsonObjectConverter.h"
#include "Json.h"
#include "TimerManager.h"
#include "Interfaces/IHttpRequest.h"
#include "HttpModule.h"
#include "Containers/Array.h"
#include "Runtime/Core/Public/GenericPlatform/GenericPlatformTime.h"

int64 UManualVRPluginBPLibrary::sessionId = 0;
FString UserToken;
FString sessionIdFromResponse;
FString BasedUrl;
bool IsTraining;
bool bIsRunning = true;
int ElapsedSeconds = 0;
int pause;
float LastTime = 0.0f;
float PauseStart = 0.0f;
float PauseEnd = 0.0f;
float PauseDuration = 0.0f;
bool bIsPause = false;
int duration;
int pauseTime;

UManualVRPluginBPLibrary::UManualVRPluginBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

bool UManualVRPluginBPLibrary::InitializeCommandLine(const FString& SourceString, FString& basedUrl, FString& userToken, FString& trainingModuleVersionId, FString& trainingCourseScheduleId, FString& privateUserId, FString& publicUserId, FString& isTraining)
{
	// Check if the SourceString is empty or consists only of whitespace characters
	if (SourceString.TrimStartAndEnd().IsEmpty())
	{
		exit(0);
	}

	TArray<FString> Substrings;
	FString Delimiter = " ";
	SourceString.ParseIntoArray(Substrings, *Delimiter, true);

	if (Substrings.Num() >= 4)
	{
		int basedUrlindex = Substrings.Find("--baseUrl");
		int userTokenindex = Substrings.Find("--userToken");
		int isTrainingindex = Substrings.Find("--isTraining");
		int trainingModuleVersionIdIndex = Substrings.Find("--trainingModuleVersionId");
		int trainingCourseScheduleIdIndex = Substrings.Find("--trainingCourseScheduleId");
		int privateUserIdIndex = Substrings.Find("--privateUserId");
		int publicUserIdIndex = Substrings.Find("--publicUserId");



		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, (TEXT("%s"), isTraining));

		if (basedUrlindex != -1 && Substrings.IsValidIndex(basedUrlindex + 1))
		{
			basedUrl = Substrings[basedUrlindex + 1];
			BasedUrl = basedUrl;
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, (TEXT("%s"), basedUrl));
		}

		if (userTokenindex != -1 && Substrings.IsValidIndex(userTokenindex + 1))
		{
			userToken = Substrings[userTokenindex + 1];
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, (TEXT("%s"), userToken));
			UserToken = userToken;
		}

		if (trainingModuleVersionIdIndex != -1 && Substrings.IsValidIndex(trainingModuleVersionIdIndex + 1))
		{
			trainingModuleVersionId = Substrings[trainingModuleVersionIdIndex + 1];
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, (TEXT("%s"), userToken));
		}

		if (trainingCourseScheduleIdIndex != -1 && Substrings.IsValidIndex(trainingCourseScheduleIdIndex + 1))
		{
			trainingCourseScheduleId = Substrings[trainingCourseScheduleIdIndex + 1];
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, (TEXT("%s"), userToken));
		}

		if (privateUserIdIndex != -1 && Substrings.IsValidIndex(privateUserIdIndex + 1))
		{
			privateUserId = Substrings[privateUserIdIndex + 1];
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, (TEXT("%s"), userToken));
		}

		if (publicUserIdIndex != -1 && Substrings.IsValidIndex(publicUserIdIndex + 1))
		{
			publicUserId = Substrings[publicUserIdIndex + 1];
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, (TEXT("%s"), userToken));
		}

		if (isTrainingindex != -1 && Substrings.IsValidIndex(isTrainingindex + 1))
		{
			isTraining = Substrings[isTrainingindex + 1];

			if (isTraining == "false")
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, (TEXT("%s"), isTraining));
				IsTraining = false;
				return false;
			}
			IsTraining = true;
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, (TEXT("%s"), isTraining));
		}
	}
	return true;
}

void UManualVRPluginBPLibrary::CreateSession(const FString& trainingModuleVersionId, const FString& trainingCourseScheduleId, const FString& privateUserId, const FString& publicUserId)
{
	FString apiBaseUrl = BasedUrl + "/api/app/training-session/start-new-training-session";

	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField("trainingModuleVersionId", trainingModuleVersionId);
	RequestBody->SetStringField("trainingCourseScheduleId", trainingCourseScheduleId);
	RequestBody->SetStringField("privateUserId", privateUserId);
	RequestBody->SetStringField("publicUserId", publicUserId);
	RequestBody->SetBoolField("isTraining", IsTraining);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetHeader("Authorization", "Bearer " + UserToken);
	HttpRequest->SetVerb("POST");
	HttpRequest->SetURL(apiBaseUrl);
	HttpRequest->SetHeader("Content-Type", "application/json");


	FString RequestBodyString;
	TSharedRef<TJsonWriter<TCHAR>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	FJsonSerializer::Serialize(RequestBody, Writer);
	HttpRequest->SetContentAsString(RequestBodyString);

	HttpRequest->OnProcessRequestComplete().BindLambda([&](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (bWasSuccessful)
			{
				TSharedPtr<FJsonObject> JsonObject;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
				FString errorCode = FString::FromInt(Response->GetResponseCode());

				if (FJsonSerializer::Deserialize(Reader, JsonObject))
				{
					if (errorCode == "200")
					{
						FString sessionID = JsonObject->GetStringField("sessionId");
						sessionIdFromResponse = sessionID;
						//sessionId = sessionIdFromResponse;
						//int64 sessionId = sessionID;
						//GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("Session ID Latest: %d"), SessionId));
						GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("Session ID: %s"), *sessionID));
						GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("Code: %s"), *errorCode));
						GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Success"));
					}

					else
					{
						FString message = JsonObject->GetObjectField("result")->GetStringField("message");
						GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Message: %s"), *message));
						GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Server Code Error: %s"), *errorCode));
					}

				}
				else
				{
					FString message = JsonObject->GetObjectField("result")->GetStringField("message");
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("Message: %s"), *message));
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Server Code Error: %s"), *errorCode));
				}

				FString responseString = Response->GetContentAsString();
				if (responseString.IsEmpty())
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("No Response: %s"), *responseString));
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Server Code Error: %s"), *errorCode));
				}
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed"));
				FGenericPlatformMisc::RequestExit(false);
			}
		});
	HttpRequest->ProcessRequest();
}

void UManualVRPluginBPLibrary::Start()
{
	bIsRunning = true;
	ElapsedSeconds = 0;
	LastTime = FPlatformTime::Seconds();
}

void UManualVRPluginBPLibrary::Pause()
{
	bIsPause = true;
	PauseStart = FPlatformTime::Seconds();
}

void UManualVRPluginBPLibrary::Resume()
{
	PauseEnd = FPlatformTime::Seconds();
	PauseDuration = PauseEnd - PauseStart;
	pause = static_cast<int>(PauseDuration);
}

void UManualVRPluginBPLibrary::End()
{
	bIsRunning = false;
	//return static_cast<int32>(ElapsedSeconds);

	FString apiBaseUrl = BasedUrl + "/api/app/training-session/end-session";

	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField("trainingSessionId", *sessionIdFromResponse);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(apiBaseUrl);
	HttpRequest->SetVerb("POST");
	HttpRequest->SetHeader("Content-Type", "application/json");
	HttpRequest->SetHeader("Authorization", "Bearer " + UserToken);

	FString RequestBodyString;
	TSharedRef<TJsonWriter<TCHAR>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	FJsonSerializer::Serialize(RequestBody, Writer);
	HttpRequest->SetContentAsString(RequestBodyString);
	UE_LOG(LogTemp, Warning, TEXT("request body string: %s"), *RequestBodyString);

	HttpRequest->OnProcessRequestComplete().BindLambda([&](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (!bWasSuccessful || !Response.IsValid()) {
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to make HTTP request"));
			}

			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			FString errorCode = FString::FromInt(Response->GetResponseCode());

			if (FJsonSerializer::Deserialize(Reader, JsonObject))
			{
				if (errorCode == "200")
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Success"));
				}

				else
				{
					FString message = JsonObject->GetObjectField("result")->GetStringField("message");
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Message: %s"), *message));
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Server Code Error: %s"), *errorCode));
				}
			}
			else
			{
				FString message = JsonObject->GetObjectField("result")->GetStringField("message");
				GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("Message: %s"), *message));
				GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Server Code Error: %s"), *errorCode));
			}

			FString responseString = Response->GetContentAsString();
			if (responseString.IsEmpty())
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("No Response: %s"), *responseString));
				GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Server Code Error: %s"), *errorCode));
			}
		});

	HttpRequest->ProcessRequest();
	return;
}

void UManualVRPluginBPLibrary::Update(const FString& chapterCode, const FString& procedureCode, const FString& subProcedureCode, int completionStatus)
{

	if (bIsRunning)
	{
		float DeltaTime = 0.0f;
		const float CurrentTime = FPlatformTime::Seconds();
		DeltaTime = CurrentTime - LastTime;
		LastTime = CurrentTime;

		if (bIsPause)
		{
			duration = static_cast<int>(DeltaTime);
			pauseTime = duration;
			duration = pauseTime - pause;
			bIsPause = false;
		}

		else
		{
			duration = static_cast<int>(DeltaTime);
		}

		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Duration: %d"), duration));
	}

	if (sessionIdFromResponse.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("sessionId not set")));
		return;
	}

	

	FString apiBaseUrl = BasedUrl + "/api/app/training-session/session";
	//FString updateUrl = apiBaseUrl + FString::Printf(TEXT("%lld"), sessionId) + "/update";

	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField("trainingSessionId", *sessionIdFromResponse);
	RequestBody->SetStringField("chapterCode", chapterCode);
	RequestBody->SetStringField("procedureCode", procedureCode);
	RequestBody->SetStringField("subProcedureCode", subProcedureCode);
	RequestBody->SetNumberField("timeTaken", duration);
	RequestBody->SetNumberField("completionStatus", completionStatus);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(apiBaseUrl);
	HttpRequest->SetVerb("PUT");
	HttpRequest->SetHeader("Content-Type", "application/json");
	HttpRequest->SetHeader("Authorization", "Bearer " + UserToken);

	FString RequestBodyString;
	TSharedRef<TJsonWriter<TCHAR>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	FJsonSerializer::Serialize(RequestBody, Writer);
	HttpRequest->SetContentAsString(RequestBodyString);
	UE_LOG(LogTemp, Warning, TEXT("request body string: %s"), *RequestBodyString);

	HttpRequest->OnProcessRequestComplete().BindLambda([&](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		if (!bWasSuccessful || !Response.IsValid()) {
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to make HTTP request"));
		}

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		FString errorCode = FString::FromInt(Response->GetResponseCode());

		if (FJsonSerializer::Deserialize(Reader, JsonObject))
		{
			if (errorCode == "200")
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Success"));
			}

			else
			{
				FString message = JsonObject->GetObjectField("result")->GetStringField("message");
				GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Message: %s"), *message));
				GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Server Code Error: %s"), *errorCode));
			}
		}
		else
		{
			FString message = JsonObject->GetObjectField("result")->GetStringField("message");
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("Message: %s"), *message));
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Server Code Error: %s"), *errorCode));
		}

		FString responseString = Response->GetContentAsString();
		if (responseString.IsEmpty())
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("No Response: %s"), *responseString));
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Server Code Error: %s"), *errorCode));
		}
	});
	
	HttpRequest->ProcessRequest();
	return;
}