

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "Azr_SessionSubsystem.generated.h"

UENUM(BlueprintType)
enum class EAzr_SessionType : uint8 {
	Training,
	Event,
	Course
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionResponse, const FString&, SessionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUpdateResponse, float, ScorePercentage, const FString&, SessionStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionError, const FString&, ErrorMessage);

UCLASS()
class AZUREALXR_API UAzr_SessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Starts the appropriate session type based on Launch Context */
	void StartSession();

	/** Records a physical action subprocedure to the server */
	void UpdateSubprocedure(int32 ChapterNum, int32 StepNum, int32 SubStepNum, bool bCompleted);

	/** Records a knowledge-check question answer to the server */
	void UpdateQuestion(int32 ChapterNum, int32 StepNum, int32 SubStepNum, int32 SelectedOption);

	/** Ends the current session and retrieves the final score and status */
	void EndSession();

	/** Explicitly marks the current time. Used at the start of a Step to ignore previous dead time. */
	void RecordCurrentTime();

	// --- BROADCASTS ---
	UPROPERTY()
	FOnSessionResponse OnStartSuccess;

	UPROPERTY()
	FOnSessionError OnStartFailure;

	UPROPERTY()
	FOnUpdateResponse OnUpdateSuccess;

	UPROPERTY()
	FOnSessionError OnUpdateFailure;

	UPROPERTY()
	FOnUpdateResponse OnEndSuccess;

	UPROPERTY()
	FOnSessionError OnEndFailure;

	// --- BLUEPRINT API ---
	UFUNCTION(BlueprintPure, Category = "Azureal|Session")
	bool IsExplainedMode() const { return bIsExplained; }

	UFUNCTION(BlueprintPure, Category = "Azureal|Session")
	bool IsOnlineMode() const { return bIsOnline; }

	UFUNCTION(BlueprintPure, Category = "Azureal|Session")
	FString GetCurrentSessionId() const { return SessionId; }

	UFUNCTION(BlueprintPure, Category = "Azureal|Session")
	FString GetSessionLanguage() const { return LanguageCode; }

private:
	FString AuthToken;
	FString BaseApiUrl;
	FString ModuleVersionId;
	FString SessionId;
	FString LanguageCode;

	bool bIsExplained;
	bool bIsOnline;
	EAzr_SessionType DeterminedSessionType;

	float ActiveTimestamp = 0.0f;

	void LoadLauncherContext();
	void OnStartSessionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnUpdateResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnEndSessionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};