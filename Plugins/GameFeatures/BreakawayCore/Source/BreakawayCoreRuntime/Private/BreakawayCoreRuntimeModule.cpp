// Copyright Epic Games, Inc. All Rights Reserved.

#include "BreakawayCoreRuntimeModule.h"

#define LOCTEXT_NAMESPACE "FBreakawayCoreRuntimeModule"

namespace BwayTravelGuidDiagnostics
{
	void InstallHooks();
	void UninstallHooks();
}

void FBreakawayCoreRuntimeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory;
	// the exact timing is specified in the .uplugin file per-module

	// Seamless-travel NetGUID lifecycle observer. Installed here (instead of a static
	// initializer) so module shutdown can remove every engine callback that captures it.
	BwayTravelGuidDiagnostics::InstallHooks();
}

void FBreakawayCoreRuntimeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.
	// For modules that support dynamic reloading, we call this function before unloading the module.
	BwayTravelGuidDiagnostics::UninstallHooks();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBreakawayCoreRuntimeModule, BreakawayCoreRuntime)
