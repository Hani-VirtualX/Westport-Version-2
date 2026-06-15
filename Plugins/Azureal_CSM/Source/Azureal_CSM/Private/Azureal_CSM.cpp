// Copyright Epic Games, Inc. All Rights Reserved.

#include "Azureal_CSM.h"

#define LOCTEXT_NAMESPACE "FAzureal_CSMModule"

void FAzureal_CSMModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FAzureal_CSMModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAzureal_CSMModule, Azureal_CSM)