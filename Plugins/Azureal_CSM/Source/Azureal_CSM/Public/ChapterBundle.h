

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TrainingCurriculum.h" // Your steps data asset
#include "ChapterBundle.generated.h"

USTRUCT(BlueprintType)
struct FChapterDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
    FText ChapterTitle;

    // Map Picker
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
    TSoftObjectPtr<UWorld> ChapterLevel;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
    UTrainingCurriculum* StepData;

    // --- NEW: The Manager Class for this specific chapter ---
    // You must place an Actor of this class in the actual Level file!
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
    TSubclassOf<AActor> ChapterGameManagerClass;
};

UCLASS(BlueprintType)
class AZUREAL_CSM_API UChapterBundle : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    TArray<FChapterDef> AllChapters;
};