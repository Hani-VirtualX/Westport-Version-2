// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
// Matches filename exactly
#include "Azureal_ExitForceKill.generated.h" 

// The API Macro matches your Plugin Name (Uppercase, No Spaces)
UCLASS()
class AZUREALFORCEEXIT_API UAzureal_ExitForceKill : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// The "Nuclear Option" - Instantly kills the game process (Silent)
	UFUNCTION(BlueprintCallable, Category = "Azureal")
	static void ForceKillGame();
};
