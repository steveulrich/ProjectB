// Copyright Epic Games, Inc. All Rights Reserved.

#include "BreakawayCoreRuntimeModule.h"

#define LOCTEXT_NAMESPACE "FBreakawayCoreRuntimeModule"

void FBreakawayCoreRuntimeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory;
	// the exact timing is specified in the .uplugin file per-module
}

void FBreakawayCoreRuntimeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.
	// For modules that support dynamic reloading, we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBreakawayCoreRuntimeModule, BreakawayCoreRuntime)
