

#include "Azr_Indicator.h"

AAzr_Indicator::AAzr_Indicator()
{
	PrimaryActorTick.bCanEverTick = false; // Optimisation: Visuals only, no Tick needed

	// Create a root so we can scale it
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AAzr_Indicator::BeginPlay()
{
	Super::BeginPlay();

	// Automatically start the idle animation when spawned
	OnStartIdle();
}