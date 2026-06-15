

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TrainingTypes.h" 
#include "TrainingSubStepRow.generated.h"

UCLASS()
class AZUREAL_CSM_API UTrainingSubStepRow : public UUserWidget
{
    GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, Category = "Training UI")
    void SetState(ESubStepState NewState);

protected:
  
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // Blueprint implements this to update colors/icons
    UFUNCTION(BlueprintImplementableEvent, Category = "Training UI")
    void OnStateChanged(ESubStepState NewState);

private:
    ESubStepState CurrentState;

    
    bool bIsFadingIn = false;
    float FadeSpeed = 4.0f;
};