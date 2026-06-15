

#pragma once

#include "CoreMinimal.h"
#include "TrainingTypes.generated.h" 

// --- ENUMS ---

UENUM(BlueprintType)
enum class ESubStepState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Active      UMETA(DisplayName = "Active"),
    Completed   UMETA(DisplayName = "Completed")
};

UENUM(BlueprintType)
enum class EStepType : uint8
{
    Interaction    UMETA(DisplayName = "Interaction (Cannot Skip)"),
    Explanation    UMETA(DisplayName = "Explanation (Skippable)")
};

// Tells the page whether to load standard instructions or the quiz UI
UENUM(BlueprintType)
enum class EMasterStepType : uint8
{
    Standard       UMETA(DisplayName = "Standard Instructions"),
    Quiz           UMETA(DisplayName = "Multiple Choice Quiz")
};

// --- STRUCTS ---

USTRUCT(BlueprintType)
struct FSubStepData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    EStepType Type = EStepType::Interaction;
};

// --- NEW STRUCT: Purely for a clean Quiz inspector layout ---
USTRUCT(BlueprintType)
struct FQuizAnswerData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data", meta = (DisplayName = "Answer Option Text"))
    FText AnswerText;
};

USTRUCT(BlueprintType)
struct FStepData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    EMasterStepType StepType = EMasterStepType::Standard;

    // =========================================================================
    // STANDARD STEP PROPERTIES (Only visible if Standard Instructions is chosen)
    // =========================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data", meta = (EditCondition = "StepType == EMasterStepType::Standard", EditConditionHides))
    FText StepTitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data", meta = (EditCondition = "StepType == EMasterStepType::Standard", EditConditionHides))
    TArray<FSubStepData> SubSteps;

    // =========================================================================
    // QUIZ STEP PROPERTIES (Only visible if Multiple Choice Quiz is chosen)
    // =========================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data", meta = (DisplayName = "Quiz Question", EditCondition = "StepType == EMasterStepType::Quiz", EditConditionHides))
    FText QuizTitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data", meta = (DisplayName = "Correct Answer Index", EditCondition = "StepType == EMasterStepType::Quiz", EditConditionHides))
    int32 CorrectAnswerIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data", meta = (DisplayName = "Answers List", EditCondition = "StepType == EMasterStepType::Quiz", EditConditionHides))
    TArray<FQuizAnswerData> QuizAnswers;
};

// The clean data struct used at runtime (Kept unmodified to ensure zero Blueprint breaking changes)
USTRUCT(BlueprintType)
struct FRuntimeStep
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    EMasterStepType StepType;

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    int32 DisplayNumber;

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    FText StepTitle;

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    int32 CorrectAnswerIndex;

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    TArray<FSubStepData> ActiveSubSteps;
};