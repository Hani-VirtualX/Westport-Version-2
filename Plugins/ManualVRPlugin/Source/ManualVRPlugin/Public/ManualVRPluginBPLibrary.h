// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Http.h"
#include "Json.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ManualVRPluginBPLibrary.generated.h"


USTRUCT()
struct FSop {
	GENERATED_BODY()

	UPROPERTY()
	FString sopCode;

	UPROPERTY()
	int32 duration;

	UPROPERTY()
	int32 outcome;
};

UCLASS()
class UManualVRPluginBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	UManualVRPluginBPLibrary();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ManualVR Initialize", Keywords = "Split a string"), Category = "Virtual X Plugin|ManualVR")
	static bool InitializeCommandLine(const FString& SourceString, FString& basedUrl, FString& userToken, FString& trainingModuleVersionId, FString& trainingCourseScheduleId, FString& privateUserId, FString& publicUserId, FString& isTraining);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ManualVR Create Session", Keywords = "Create Session"), Category = "Virtual X Plugin|ManualVR")
	static void CreateSession(const FString& trainingModuleVersionId, const FString& trainingCourseScheduleId, const FString& privateUserId, const FString& publicUserId);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ManualVR Start", Keywords = "Start"), Category = "Virtual X Plugin|ManualVR")
	static void Start();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ManualVR Pause", Keywords = "Pause"), Category = "Virtual X Plugin|ManualVR")
	static void Pause();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ManualVR Unpause", Keywords = "Resume"), Category = "Virtual X Plugin|ManualVR")
	static void Resume();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ManualVR End", Keywords = "End"), Category = "Virtual X Plugin|ManualVR")
	static void End();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ManualVR Update", Keywords = "Update"), Category = "Virtual X Plugin|ManualVR")
	static void Update(const FString& chapterCode, const FString& procedureCode, const FString& subProcedureCode, int completionStatus);


private:
	void OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	static int64 sessionId;
	static bool bIsTiming;
	static FDateTime startTime;
	static int32 elapsedTimeInSeconds;
};
