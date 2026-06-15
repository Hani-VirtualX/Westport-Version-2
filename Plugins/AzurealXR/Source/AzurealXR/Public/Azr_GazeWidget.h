

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Azr_GazeWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;

UCLASS(Abstract)
class AZUREALXR_API UAzr_GazeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintReadWrite, Category = "Azureal|UI", meta = (BindWidget))
	UImage* ProgressImage;

	
	UPROPERTY(BlueprintReadWrite, Category = "Azureal|UI", meta = (BindWidget))
	UImage* GazeReticleIcon;

	UFUNCTION(BlueprintCallable, Category = "Azureal|UI")
	void SetProgress(float InProgress);

protected:
	virtual void NativeConstruct() override;

private:
	// Cached reference to the material so we don't recreate it every frame
	UPROPERTY()
	UMaterialInstanceDynamic* ProgressMID;
};