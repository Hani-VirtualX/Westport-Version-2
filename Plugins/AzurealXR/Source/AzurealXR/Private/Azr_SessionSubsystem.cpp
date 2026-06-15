

#include "Azr_SessionSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

void UAzr_SessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadLauncherContext();

	FString EventUserId = FPlatformMisc::GetEnvironmentVariable(TEXT("AZUREAL_EVENT_USER_ID"));
	FString EventId = FPlatformMisc::GetEnvironmentVariable(TEXT("AZUREAL_EVENT_ID"));
	FString EnrollmentId = FPlatformMisc::GetEnvironmentVariable(TEXT("AZUREAL_COURSE_ENROLLMENT_ID"));

	if (!EventUserId.IsEmpty() && !EventId.IsEmpty()) DeterminedSessionType = EAzr_SessionType::Event;
	else if (!EnrollmentId.IsEmpty()) DeterminedSessionType = EAzr_SessionType::Course;
	else DeterminedSessionType = EAzr_SessionType::Training;
}

void UAzr_SessionSubsystem::LoadLauncherContext()
{
	AuthToken = FPlatformMisc::GetEnvironmentVariable(TEXT("AZUREAL_TOKEN"));
	BaseApiUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("AZUREAL_API_URL"));
	ModuleVersionId = FPlatformMisc::GetEnvironmentVariable(TEXT("AZUREAL_MODULE_VERSION_ID"));
	LanguageCode = FPlatformMisc::GetEnvironmentVariable(TEXT("AZUREAL_LANGUAGE"));

	FString IsExplainedStr = FPlatformMisc::GetEnvironmentVariable(TEXT("AZUREAL_IS_EXPLAINED"));
	bIsExplained = IsExplainedStr.Equals(TEXT("true"), ESearchCase::IgnoreCase);

	FString IsOnlineStr = FPlatformMisc::GetEnvironmentVariable(TEXT("AZUREAL_IS_ONLINE"));
	bIsOnline = !IsOnlineStr.Equals(TEXT("false"), ESearchCase::IgnoreCase);

	if (bIsOnline && (AuthToken.IsEmpty() || BaseApiUrl.IsEmpty() || ModuleVersionId.IsEmpty()))
	{
		UE_LOG(LogTemp, Warning, TEXT("AzurealXR: Missing critical launch variables. Falling back to Offline Mode."));
		bIsOnline = false;
	}
}

void UAzr_SessionSubsystem::StartSession()
{
	if (!bIsOnline) {
		OnStartFailure.Broadcast(TEXT("Offline Mode: Tracking Disabled."));
		return;
	}

	FString Path = TEXT("/training-sessions");
	if (DeterminedSessionType == EAzr_SessionType::Event) Path = TEXT("/event-sessions");
	else if (DeterminedSessionType == EAzr_SessionType::Course) Path = TEXT("/course-sessions");

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UAzr_SessionSubsystem::OnStartSessionResponse);
	Request->SetURL(BaseApiUrl + Path);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));

	TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject());
	JsonObj->SetStringField(TEXT("moduleVersionId"), ModuleVersionId);
	JsonObj->SetBoolField(TEXT("isExplained"), bIsExplained);

	if (DeterminedSessionType == EAzr_SessionType::Event) {
		JsonObj->SetStringField(TEXT("eventUserId"), FPlatformMisc::GetEnvironmentVariable(TEXT("AZUREAL_EVENT_USER_ID")));
		JsonObj->SetStringField(TEXT("eventId"), FPlatformMisc::GetEnvironmentVariable(TEXT("AZUREAL_EVENT_ID")));
	}
	else if (DeterminedSessionType == EAzr_SessionType::Course) {
		JsonObj->SetStringField(TEXT("courseEnrollmentId"), FPlatformMisc::GetEnvironmentVariable(TEXT("AZUREAL_COURSE_ENROLLMENT_ID")));
	}

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	Request->SetContentAsString(Body);
	Request->ProcessRequest();
}

void UAzr_SessionSubsystem::OnStartSessionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200) {
		OnStartFailure.Broadcast(Response.IsValid() ? Response->GetContentAsString() : TEXT("Network Error"));
		return;
	}

	TSharedPtr<FJsonObject> JsonResponse;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid()) {
		FString ParsedId;
		if (JsonResponse->TryGetStringField(TEXT("id"), ParsedId))
		{
			SessionId = ParsedId;
			RecordCurrentTime();
			OnStartSuccess.Broadcast(SessionId);
		}
		else
		{
			OnStartFailure.Broadcast(TEXT("Invalid Server Response: Missing Session ID."));
		}
	}
	else
	{
		OnStartFailure.Broadcast(TEXT("Failed to parse Start Session JSON response. Check Server payload."));
	}
}

void UAzr_SessionSubsystem::RecordCurrentTime()
{
	if (UWorld* World = GetWorld())
	{
		ActiveTimestamp = World->GetTimeSeconds();
	}
}

void UAzr_SessionSubsystem::UpdateSubprocedure(int32 ChapterNum, int32 StepNum, int32 SubStepNum, bool bCompleted)
{
	if (!bIsOnline) {
		OnUpdateFailure.Broadcast(TEXT("Offline Mode: Subprocedure Not Recorded."));
		return;
	}
	if (SessionId.IsEmpty()) {
		OnUpdateFailure.Broadcast(TEXT("Error: No Active Session ID. Call StartSession first."));
		return;
	}

	int32 CalculatedTime = 1;
	if (UWorld* World = GetWorld())
	{
		float CurrentTime = World->GetTimeSeconds();
		float TimeDifference = CurrentTime - ActiveTimestamp;
		CalculatedTime = FMath::Max(1, FMath::RoundToInt(TimeDifference));
		ActiveTimestamp = CurrentTime;
	}

	FString Path = TEXT("/training-sessions/") + SessionId + TEXT("/update");
	if (DeterminedSessionType == EAzr_SessionType::Event) Path = TEXT("/event-sessions/") + SessionId + TEXT("/update");
	else if (DeterminedSessionType == EAzr_SessionType::Course) Path = TEXT("/course-sessions/") + SessionId + TEXT("/update");

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UAzr_SessionSubsystem::OnUpdateResponseReceived);
	Request->SetURL(BaseApiUrl + Path);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));

	TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject());
	JsonObj->SetNumberField(TEXT("chapterNumber"), ChapterNum);
	JsonObj->SetNumberField(TEXT("stepNumber"), StepNum);
	JsonObj->SetNumberField(TEXT("subStepNumber"), SubStepNum);
	JsonObj->SetNumberField(TEXT("timeTakenInSeconds"), CalculatedTime);
	JsonObj->SetBoolField(TEXT("completed"), bCompleted);

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	Request->SetContentAsString(Body);
	Request->ProcessRequest();
}

void UAzr_SessionSubsystem::UpdateQuestion(int32 ChapterNum, int32 StepNum, int32 SubStepNum, int32 SelectedOption)
{
	if (!bIsOnline) {
		OnUpdateFailure.Broadcast(TEXT("Offline Mode: Question Not Recorded."));
		return;
	}
	if (SessionId.IsEmpty()) {
		OnUpdateFailure.Broadcast(TEXT("Error: No Active Session ID. Call StartSession first."));
		return;
	}

	RecordCurrentTime();

	FString Path = TEXT("/training-sessions/") + SessionId + TEXT("/update-question");
	if (DeterminedSessionType == EAzr_SessionType::Event) Path = TEXT("/event-sessions/") + SessionId + TEXT("/update-question");
	else if (DeterminedSessionType == EAzr_SessionType::Course) Path = TEXT("/course-sessions/") + SessionId + TEXT("/update-question");

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UAzr_SessionSubsystem::OnUpdateResponseReceived);
	Request->SetURL(BaseApiUrl + Path);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));

	TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject());
	JsonObj->SetNumberField(TEXT("chapterNumber"), ChapterNum);
	JsonObj->SetNumberField(TEXT("stepNumber"), StepNum);
	JsonObj->SetNumberField(TEXT("subStepNumber"), SubStepNum);

	if (SelectedOption > 0) {
		JsonObj->SetNumberField(TEXT("selectedOptionNumber"), SelectedOption);
	}
	else {
		JsonObj->SetField(TEXT("selectedOptionNumber"), MakeShareable(new FJsonValueNull()));
	}

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	Request->SetContentAsString(Body);
	Request->ProcessRequest();
}

void UAzr_SessionSubsystem::OnUpdateResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200) {
		OnUpdateFailure.Broadcast(Response.IsValid() ? Response->GetContentAsString() : TEXT("Network Error"));
		return;
	}

	TSharedPtr<FJsonObject> JsonResponse;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (FJsonSerializer::Deserialize(Reader, JsonResponse)) {
		float Score = 0.0f;
		FString Status = TEXT("InProgress");

		JsonResponse->TryGetNumberField(TEXT("scorePercentage"), Score);
		JsonResponse->TryGetStringField(TEXT("sessionStatus"), Status);

		OnUpdateSuccess.Broadcast(Score, Status);
	}
	else {
		OnUpdateFailure.Broadcast(TEXT("Failed to parse Update response."));
	}
}

// ==============================================================================
// END SESSION LOGIC
// ==============================================================================

void UAzr_SessionSubsystem::EndSession()
{
	if (!bIsOnline) {
		OnEndFailure.Broadcast(TEXT("Offline Mode: Session Ended Locally."));
		return;
	}
	if (SessionId.IsEmpty()) {
		OnEndFailure.Broadcast(TEXT("Error: No Active Session ID to end."));
		return;
	}

	FString Path = TEXT("/training-sessions/") + SessionId + TEXT("/end");
	if (DeterminedSessionType == EAzr_SessionType::Event) Path = TEXT("/event-sessions/") + SessionId + TEXT("/end");
	else if (DeterminedSessionType == EAzr_SessionType::Course) Path = TEXT("/course-sessions/") + SessionId + TEXT("/end");

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UAzr_SessionSubsystem::OnEndSessionResponse);
	Request->SetURL(BaseApiUrl + Path);

	// API Requires PUT for End
	Request->SetVerb(TEXT("PUT"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));

	// API Requires just {"id": sessionId} in the body
	TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject());
	JsonObj->SetStringField(TEXT("id"), SessionId);

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	Request->SetContentAsString(Body);
	Request->ProcessRequest();
}

void UAzr_SessionSubsystem::OnEndSessionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200) {
		OnEndFailure.Broadcast(Response.IsValid() ? Response->GetContentAsString() : TEXT("Network Error"));
		return;
	}

	TSharedPtr<FJsonObject> JsonResponse;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (FJsonSerializer::Deserialize(Reader, JsonResponse)) {
		float FinalScore = 0.0f;
		FString FinalStatus = TEXT("Completed");

		JsonResponse->TryGetNumberField(TEXT("scorePercentage"), FinalScore);
		JsonResponse->TryGetStringField(TEXT("sessionStatus"), FinalStatus);

		// Clear the Session ID now that it's officially over
		SessionId = TEXT("");

		OnEndSuccess.Broadcast(FinalScore, FinalStatus);
	}
	else {
		OnEndFailure.Broadcast(TEXT("Failed to parse End response."));
	}
}