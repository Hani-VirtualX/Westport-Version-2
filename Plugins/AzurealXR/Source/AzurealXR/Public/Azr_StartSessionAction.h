

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Azr_StartSessionAction.generated.h"

class UAzr_SessionSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStartSessionPin, FString, SessionId, FString, Error);

/**
 * UAzr_StartSessionAction
 * Async Blueprint node to start tracking an Azureal Session.
 */
UCLASS()
class AZUREALXR_API UAzr_StartSessionAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Starts the Azureal Session and waits for a server response. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Azureal - Start Session", BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Azureal|Session")
	static UAzr_StartSessionAction* StartAzurealSession(UObject* WorldContextObject);

	UPROPERTY(BlueprintAssignable)
	FStartSessionPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FStartSessionPin OnFailure;

	virtual void Activate() override;

private:
	UPROPERTY()
	UObject* WorldContext;

	// Cache the subsystem so we can unbind later to prevent memory leaks
	TWeakObjectPtr<UAzr_SessionSubsystem> CachedSubsystem;

	UFUNCTION()
	void HandleSuccess(const FString& Id);

	UFUNCTION()
	void HandleFailure(const FString& Error);
};