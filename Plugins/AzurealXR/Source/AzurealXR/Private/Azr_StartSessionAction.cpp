

#include "Azr_StartSessionAction.h"
#include "Azr_SessionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

UAzr_StartSessionAction* UAzr_StartSessionAction::StartAzurealSession(UObject* WorldContextObject)
{
	UAzr_StartSessionAction* Action = NewObject<UAzr_StartSessionAction>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAzr_StartSessionAction::Activate()
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContext))
	{
		if (UAzr_SessionSubsystem* Sub = GI->GetSubsystem<UAzr_SessionSubsystem>())
		{
			// Cache the subsystem for cleanup later
			CachedSubsystem = Sub;

			// Bind to the Subsystem's delegates
			Sub->OnStartSuccess.AddDynamic(this, &UAzr_StartSessionAction::HandleSuccess);
			Sub->OnStartFailure.AddDynamic(this, &UAzr_StartSessionAction::HandleFailure);

			// Fire the HTTP request
			Sub->StartSession();
			return;
		}
	}

	// Failsafe if the GameInstance or Subsystem is somehow missing
	OnFailure.Broadcast(TEXT(""), TEXT("Internal System Error: Subsystem not found."));
	SetReadyToDestroy();
}

void UAzr_StartSessionAction::HandleSuccess(const FString& Id)
{
	// CLEANUP: Prevent ghost executions on future calls
	if (CachedSubsystem.IsValid())
	{
		CachedSubsystem->OnStartSuccess.RemoveDynamic(this, &UAzr_StartSessionAction::HandleSuccess);
		CachedSubsystem->OnStartFailure.RemoveDynamic(this, &UAzr_StartSessionAction::HandleFailure);
	}

	OnSuccess.Broadcast(Id, TEXT(""));
	SetReadyToDestroy();
}

void UAzr_StartSessionAction::HandleFailure(const FString& Error)
{
	// CLEANUP: Prevent ghost executions on future calls
	if (CachedSubsystem.IsValid())
	{
		CachedSubsystem->OnStartSuccess.RemoveDynamic(this, &UAzr_StartSessionAction::HandleSuccess);
		CachedSubsystem->OnStartFailure.RemoveDynamic(this, &UAzr_StartSessionAction::HandleFailure);
	}

	OnFailure.Broadcast(TEXT(""), Error);
	SetReadyToDestroy();
}