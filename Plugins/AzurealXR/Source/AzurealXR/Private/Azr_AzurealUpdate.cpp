

#include "Azr_AzurealUpdate.h"
#include "Azr_SessionSubsystem.h"
#include "Kismet/GameplayStatics.h"

// ==============================================================================
// PHYSICAL ACTION NODE IMPLEMENTATION
// ==============================================================================

UAzr_AzurealUpdate* UAzr_AzurealUpdate::AzurealUpdate(UObject* WorldContextObject, int32 ChapterNumber, int32 StepNumber, int32 SubStepNumber, bool bCompleted)
{
	UAzr_AzurealUpdate* Action = NewObject<UAzr_AzurealUpdate>();
	Action->WorldContext = WorldContextObject;
	Action->CachedChapter = ChapterNumber;
	Action->CachedStep = StepNumber;
	Action->CachedSubStep = SubStepNumber;
	Action->bCachedCompleted = bCompleted;
	return Action;
}

void UAzr_AzurealUpdate::Activate()
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContext))
	{
		if (UAzr_SessionSubsystem* Sub = GI->GetSubsystem<UAzr_SessionSubsystem>())
		{
			CachedSubsystem = Sub;
			Sub->OnUpdateSuccess.AddDynamic(this, &UAzr_AzurealUpdate::HandleSuccess);
			Sub->OnUpdateFailure.AddDynamic(this, &UAzr_AzurealUpdate::HandleFailure);
			Sub->UpdateSubprocedure(CachedChapter, CachedStep, CachedSubStep, bCachedCompleted);
			return;
		}
	}
	OnFailure.Broadcast(0.0f, TEXT(""), TEXT("Internal System Error: Subsystem not found."));
	SetReadyToDestroy();
}

void UAzr_AzurealUpdate::HandleSuccess(float ScorePercentage, const FString& SessionStatus)
{
	if (CachedSubsystem.IsValid())
	{
		CachedSubsystem->OnUpdateSuccess.RemoveDynamic(this, &UAzr_AzurealUpdate::HandleSuccess);
		CachedSubsystem->OnUpdateFailure.RemoveDynamic(this, &UAzr_AzurealUpdate::HandleFailure);
	}
	OnSuccess.Broadcast(ScorePercentage, SessionStatus, TEXT(""));
	SetReadyToDestroy();
}

void UAzr_AzurealUpdate::HandleFailure(const FString& Error)
{
	if (CachedSubsystem.IsValid())
	{
		CachedSubsystem->OnUpdateSuccess.RemoveDynamic(this, &UAzr_AzurealUpdate::HandleSuccess);
		CachedSubsystem->OnUpdateFailure.RemoveDynamic(this, &UAzr_AzurealUpdate::HandleFailure);
	}
	OnFailure.Broadcast(0.0f, TEXT(""), Error);
	SetReadyToDestroy();
}

void UAzr_AzurealUpdate::RecordCurrentTime(UObject* WorldContextObject)
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject))
	{
		if (UAzr_SessionSubsystem* Sub = GI->GetSubsystem<UAzr_SessionSubsystem>())
		{
			Sub->RecordCurrentTime();
		}
	}
}


// ==============================================================================
// QUIZ QUESTION NODE IMPLEMENTATION
// ==============================================================================

UAzr_AzurealQuizUpdate* UAzr_AzurealQuizUpdate::AzurealQuizUpdate(UObject* WorldContextObject, int32 ChapterNumber, int32 StepNumber, int32 SubStepNumber, int32 SelectedOptionNumber)
{
	UAzr_AzurealQuizUpdate* Action = NewObject<UAzr_AzurealQuizUpdate>();
	Action->WorldContext = WorldContextObject;
	Action->CachedChapter = ChapterNumber;
	Action->CachedStep = StepNumber;
	Action->CachedSubStep = SubStepNumber;
	Action->CachedOption = SelectedOptionNumber;
	return Action;
}

void UAzr_AzurealQuizUpdate::Activate()
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContext))
	{
		if (UAzr_SessionSubsystem* Sub = GI->GetSubsystem<UAzr_SessionSubsystem>())
		{
			CachedSubsystem = Sub;
			Sub->OnUpdateSuccess.AddDynamic(this, &UAzr_AzurealQuizUpdate::HandleSuccess);
			Sub->OnUpdateFailure.AddDynamic(this, &UAzr_AzurealQuizUpdate::HandleFailure);
			Sub->UpdateQuestion(CachedChapter, CachedStep, CachedSubStep, CachedOption);
			return;
		}
	}
	OnFailure.Broadcast(0.0f, TEXT(""), TEXT("Internal System Error: Subsystem not found."));
	SetReadyToDestroy();
}

void UAzr_AzurealQuizUpdate::HandleSuccess(float ScorePercentage, const FString& SessionStatus)
{
	if (CachedSubsystem.IsValid())
	{
		CachedSubsystem->OnUpdateSuccess.RemoveDynamic(this, &UAzr_AzurealQuizUpdate::HandleSuccess);
		CachedSubsystem->OnUpdateFailure.RemoveDynamic(this, &UAzr_AzurealQuizUpdate::HandleFailure);
	}
	OnSuccess.Broadcast(ScorePercentage, SessionStatus, TEXT(""));
	SetReadyToDestroy();
}

void UAzr_AzurealQuizUpdate::HandleFailure(const FString& Error)
{
	if (CachedSubsystem.IsValid())
	{
		CachedSubsystem->OnUpdateSuccess.RemoveDynamic(this, &UAzr_AzurealQuizUpdate::HandleSuccess);
		CachedSubsystem->OnUpdateFailure.RemoveDynamic(this, &UAzr_AzurealQuizUpdate::HandleFailure);
	}
	OnFailure.Broadcast(0.0f, TEXT(""), Error);
	SetReadyToDestroy();
}


// ==============================================================================
// END SESSION NODE IMPLEMENTATION
// ==============================================================================

UAzr_AzurealEndSession* UAzr_AzurealEndSession::AzurealEndSession(UObject* WorldContextObject)
{
	UAzr_AzurealEndSession* Action = NewObject<UAzr_AzurealEndSession>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAzr_AzurealEndSession::Activate()
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContext))
	{
		if (UAzr_SessionSubsystem* Sub = GI->GetSubsystem<UAzr_SessionSubsystem>())
		{
			CachedSubsystem = Sub;
			Sub->OnEndSuccess.AddDynamic(this, &UAzr_AzurealEndSession::HandleSuccess);
			Sub->OnEndFailure.AddDynamic(this, &UAzr_AzurealEndSession::HandleFailure);
			Sub->EndSession();
			return;
		}
	}
	OnFailure.Broadcast(0.0f, TEXT(""), TEXT("Internal System Error: Subsystem not found."));
	SetReadyToDestroy();
}

void UAzr_AzurealEndSession::HandleSuccess(float ScorePercentage, const FString& SessionStatus)
{
	if (CachedSubsystem.IsValid())
	{
		CachedSubsystem->OnEndSuccess.RemoveDynamic(this, &UAzr_AzurealEndSession::HandleSuccess);
		CachedSubsystem->OnEndFailure.RemoveDynamic(this, &UAzr_AzurealEndSession::HandleFailure);
	}
	OnSuccess.Broadcast(ScorePercentage, SessionStatus, TEXT(""));
	SetReadyToDestroy();
}

void UAzr_AzurealEndSession::HandleFailure(const FString& Error)
{
	if (CachedSubsystem.IsValid())
	{
		CachedSubsystem->OnEndSuccess.RemoveDynamic(this, &UAzr_AzurealEndSession::HandleSuccess);
		CachedSubsystem->OnEndFailure.RemoveDynamic(this, &UAzr_AzurealEndSession::HandleFailure);
	}
	OnFailure.Broadcast(0.0f, TEXT(""), Error);
	SetReadyToDestroy();
}