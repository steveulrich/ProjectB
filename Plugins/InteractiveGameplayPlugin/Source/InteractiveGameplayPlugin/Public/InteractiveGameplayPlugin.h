// InteractiveGameplayPlugin.h
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FInteractiveGameplayPluginModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};