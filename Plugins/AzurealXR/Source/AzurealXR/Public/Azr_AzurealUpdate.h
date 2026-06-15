

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Azr_AzurealUpdate.generated.h"

class UAzr_SessionSubsystem;

// Shared delegate for Physical Actions, Quizzes, and End Session
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAzurealUpdatePin, float, ScorePercentage, FString, SessionStatus, FString, ErrorMessage);

// ==============================================================================
// PHYSICAL ACTION NODE
// ==============================================================================

UCLASS()
class AZUREALXR_API UAzr_AzurealUpdate : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Records a completed or failed action step to the Azureal Server. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Azureal - Record Action", BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Azureal|Session")
	static UAzr_AzurealUpdate* AzurealUpdate(UObject* WorldContextObject, int32 ChapterNumber, int32 StepNumber, int32 SubStepNumber, bool bCompleted);

	/** Explicitly marks the current time. Used at the start of a Step to ignore previous dead time. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Azureal - Record Time", WorldContext = "WorldContextObject"), Category = "Azureal|Session")
	static void RecordCurrentTime(UObject* WorldContextObject);

	UPROPERTY(BlueprintAssignable)
	FAzurealUpdatePin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FAzurealUpdatePin OnFailure;

	virtual void Activate() override;

private:
	UPROPERTY()
	UObject* WorldContext;

	int32 CachedChapter;
	int32 CachedStep;
	int32 CachedSubStep;
	bool bCachedCompleted;

	TWeakObjectPtr<UAzr_SessionSubsystem> CachedSubsystem;

	UFUNCTION()
	void HandleSuccess(float ScorePercentage, const FString& SessionStatus);

	UFUNCTION()
	void HandleFailure(const FString& Error);
};


// ==============================================================================
// QUIZ QUESTION NODE
// ==============================================================================

UCLASS()
class AZUREALXR_API UAzr_AzurealQuizUpdate : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Records a quiz answer to the Azureal Server. Pass 0 for SelectedOptionNumber if unanswered. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Azureal - Quiz Update", BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Azureal|Session")
	static UAzr_AzurealQuizUpdate* AzurealQuizUpdate(UObject* WorldContextObject, int32 ChapterNumber, int32 StepNumber, int32 SubStepNumber, int32 SelectedOptionNumber);

	UPROPERTY(BlueprintAssignable)
	FAzurealUpdatePin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FAzurealUpdatePin OnFailure;

	virtual void Activate() override;

private:
	UPROPERTY()
	UObject* WorldContext;

	int32 CachedChapter;
	int32 CachedStep;
	int32 CachedSubStep;
	int32 CachedOption;

	TWeakObjectPtr<UAzr_SessionSubsystem> CachedSubsystem;

	UFUNCTION()
	void HandleSuccess(float ScorePercentage, const FString& SessionStatus);

	UFUNCTION()
	void HandleFailure(const FString& Error);
};


// ==============================================================================
// END SESSION NODE
// ==============================================================================

/**
 * UAzr_AzurealEndSession
 * Async Blueprint node to end the session and fetch the final grade.
 */
UCLASS()
class AZUREALXR_API UAzr_AzurealEndSession : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Ends the current Azureal session and returns the final score and pass/fail status. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Azureal - End Session", BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Azureal|Session")
	static UAzr_AzurealEndSession* AzurealEndSession(UObject* WorldContextObject);

	UPROPERTY(BlueprintAssignable)
	FAzurealUpdatePin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FAzurealUpdatePin OnFailure;

	virtual void Activate() override;

private:
	UPROPERTY()
	UObject* WorldContext;

	TWeakObjectPtr<UAzr_SessionSubsystem> CachedSubsystem;

	UFUNCTION()
	void HandleSuccess(float ScorePercentage, const FString& SessionStatus);

	UFUNCTION()
	void HandleFailure(const FString& Error);
};