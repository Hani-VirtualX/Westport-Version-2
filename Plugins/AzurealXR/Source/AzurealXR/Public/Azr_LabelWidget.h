

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Azr_LabelWidget.generated.h"

// Forward Declaration
class UTextBlock;


UCLASS(Abstract)
class AZUREALXR_API UAzr_LabelWidget : public UUserWidget {
	GENERATED_BODY()

public:
	// --- PUBLIC API FOR THE COMPONENT TO CALL ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Label API")
	void SetLabelText(const FText& InText);

protected:
	// --- EXACT UMG BINDINGS ---
	UPROPERTY(BlueprintReadWrite, Category = "Azureal|UI", meta = (BindWidget))
	UTextBlock* LabelTextBlock;
};