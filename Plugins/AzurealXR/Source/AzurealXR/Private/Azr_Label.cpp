

#include "Azr_Label.h"
#include "Azr_LabelWidget.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

UAzr_Label::UAzr_Label()
{
	// We need tick for billboarding, but we keep it asleep until the label is actually enabled
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	InteractID = 1;
	bIsActive = false;
}

void UAzr_Label::BeginPlay()
{
	Super::BeginPlay();

	// Shut down all target widgets at start
	for (const FAzr_LabelPayload& Payload : LabelPayloads)
	{
		if (UWidgetComponent* WidgetComp = FindWidgetByName(Payload.WidgetName))
		{
			WidgetComp->SetVisibility(false);
			WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			WidgetComp->SetCollisionProfileName(FName("NoCollision"));
		}
	}
}

void UAzr_Label::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsActive) DisableLabel();
	Super::EndPlay(EndPlayReason);
}

// --- INTERFACE ---

void UAzr_Label::EnableLabel()
{
	if (bIsActive) return;
	bIsActive = true;

	// Clear out our cache just to be safe
	ActiveWidgetComps.Empty();

	// Loop through the designer's array
	for (const FAzr_LabelPayload& Payload : LabelPayloads)
	{
		if (UWidgetComponent* WidgetComp = FindWidgetByName(Payload.WidgetName))
		{
			// Inject the text into the widget blueprint
			if (UAzr_LabelWidget* LabelUI = Cast<UAzr_LabelWidget>(WidgetComp->GetUserWidgetObject()))
			{
				LabelUI->SetLabelText(Payload.LabelText);
			}

			// Wake up the visual component
			WidgetComp->SetVisibility(true);

			// Labels are just for reading, so we enforce no collision
			WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			// Cache it so our Tick can easily rotate it
			ActiveWidgetComps.Add(WidgetComp);
		}
	}

	// Wake up the component so the billboarding math starts running
	SetComponentTickEnabled(true);
}

void UAzr_Label::DisableLabel()
{
	if (!bIsActive) return;
	bIsActive = false;

	// Hide everything we cached
	for (UWidgetComponent* WidgetComp : ActiveWidgetComps)
	{
		if (WidgetComp)
		{
			WidgetComp->SetVisibility(false);
		}
	}

	ActiveWidgetComps.Empty();

	// Put the component back to sleep to save frame rate
	SetComponentTickEnabled(false);
}

// --- CORE TICK (BILLBOARDING) ---

void UAzr_Label::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsActive || ActiveWidgetComps.Num() == 0) return;

	// Billboard all active widgets to face the player's camera smoothly
	if (APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		FVector CamLoc = CamManager->GetCameraLocation();

		for (UWidgetComponent* WidgetComp : ActiveWidgetComps)
		{
			if (WidgetComp)
			{
				FVector StartLoc = WidgetComp->GetComponentLocation();
				FRotator NewRot = UKismetMathLibrary::FindLookAtRotation(StartLoc, CamLoc);
				WidgetComp->SetWorldRotation(NewRot);
			}
		}
	}
}

// --- UTILS ---

UWidgetComponent* UAzr_Label::FindWidgetByName(FName Name)
{
	if (Name.IsNone() || !GetOwner()) return nullptr;

	TArray<UWidgetComponent*> Comps;
	GetOwner()->GetComponents<UWidgetComponent>(Comps);

	for (UWidgetComponent* Comp : Comps)
	{
		if (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString()))
		{
			return Comp;
		}
	}
	return nullptr;
}