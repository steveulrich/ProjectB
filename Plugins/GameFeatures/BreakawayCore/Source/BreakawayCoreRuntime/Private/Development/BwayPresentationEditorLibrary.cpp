#include "Development/BwayPresentationEditorLibrary.h"

#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "NiagaraUserRedirectionParameterStore.h"

namespace
{
bool CanAccessNiagaraAsset(const UNiagaraSystem* System, FName ParameterName)
{
	if (!GIsEditor || !GEngine || !IsValid(System) || !System->IsAsset() ||
		!ParameterName.ToString().StartsWith(TEXT("User.")))
	{
		return false;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE)
		{
			return false;
		}
	}
	return true;
}
}
#endif

bool UBwayPresentationEditorLibrary::ReadExistingNiagaraUserFloat(UNiagaraSystem* System, FName ParameterName, float& OutValue)
{
	OutValue = 0.f;
#if WITH_EDITOR
	if (!CanAccessNiagaraAsset(System, ParameterName)) return false;
	const FNiagaraVariable Variable(FNiagaraTypeDefinition::GetFloatDef(), ParameterName);
	const FNiagaraUserRedirectionParameterStore& Parameters = System->GetExposedParameters();
	if (Parameters.IndexOf(Variable) == INDEX_NONE) return false;
	const float Value = Parameters.GetParameterValue<float>(Variable);
	if (!FMath::IsFinite(Value)) return false;
	OutValue = Value;
	return true;
#else
	return false;
#endif
}

bool UBwayPresentationEditorLibrary::SetExistingNiagaraUserFloat(UNiagaraSystem* System, FName ParameterName, float Value,
	float& OutPreviousValue, float& OutReadbackValue)
{
	OutPreviousValue = 0.f;
	OutReadbackValue = 0.f;
#if WITH_EDITOR
	if (!FMath::IsFinite(Value) || !ReadExistingNiagaraUserFloat(System, ParameterName, OutPreviousValue)) return false;
	OutReadbackValue = OutPreviousValue;
	if (Value == OutPreviousValue) return true;
	const FNiagaraVariable Variable(FNiagaraTypeDefinition::GetFloatDef(), ParameterName);
	System->Modify();
	if (!System->GetExposedParameters().SetParameterValue<float>(Value, Variable, false)) return false;
	System->PostEditChange();
	System->MarkPackageDirty();
	return ReadExistingNiagaraUserFloat(System, ParameterName, OutReadbackValue) && OutReadbackValue == Value;
#else
	return false;
#endif
}

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "EdGraph/EdGraphPin.h"
#include "Factories/Factory.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystemFactoryNew.h"
#include "UObject/Class.h"
#include "UObject/Package.h"

namespace
{
constexpr TCHAR CompactHitPath[] = TEXT("/BreakawayCore/Effects/Combat/NS_BW_HitConfirmCompact.NS_BW_HitConfirmCompact");

struct FCompactHitValue
{
	UNiagaraScript* Script = nullptr;
	FNiagaraVariable Variable;
	int32 Target = INDEX_NONE;
	float PreviousFloat = 0.f;
	int32 PreviousInt = 0;
};

bool CompactHitFailure(FString& Report, const FString& Reason)
{
	Report += TEXT("failure=") + Reason + TEXT("\n");
	return false;
}

bool CompactHitEnumPin(UNiagaraNodeFunctionCall* Node, const TCHAR* PinName,
	const TCHAR* EnumPath, const TCHAR* ExpectedDisplay, FString& Report)
{
	const UEdGraphPin* Pin = Node->FindPin(FName(PinName), EGPD_Input);
	const UEnum* Enum = Pin ? Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get()) : nullptr;
	if (!Pin || !Enum || Pin->LinkedTo.Num() != 0 || Enum->GetPathName() != EnumPath)
		return CompactHitFailure(Report, FString::Printf(TEXT("Unexpected or linked enum pin %s"), PinName));
	const int64 Value = Enum->GetValueByNameString(Pin->DefaultValue);
	if (Value == INDEX_NONE) return CompactHitFailure(Report, TEXT("Unresolved enum value"));
	const FString Display = Enum->GetDisplayNameTextByValue(Value).ToString();
	Report += FString::Printf(TEXT("enum=%s type=%s stored=%s display=%s\n"),
		PinName, *Enum->GetPathName(), *Pin->DefaultValue, *Display);
	return Display.Equals(ExpectedDisplay, ESearchCase::IgnoreCase) ||
		CompactHitFailure(Report, FString::Printf(TEXT("%s must be %s"), PinName, ExpectedDisplay));
}

bool CompactHitBoolPin(UNiagaraNodeFunctionCall* Node, const TCHAR* PinName, bool bExpected, FString& Report)
{
	const UEdGraphPin* Pin = Node->FindPin(FName(PinName), EGPD_Input);
	if (!Pin || Pin->LinkedTo.Num() != 0 || !Pin->DefaultValue.Equals(bExpected ? TEXT("true") : TEXT("false"), ESearchCase::IgnoreCase))
		return CompactHitFailure(Report, FString::Printf(TEXT("Unexpected or linked bool pin %s"), PinName));
	return true;
}

bool CompactHitReady(UNiagaraSystem* System, FString& Report)
{
	const bool bReady = !System->HasOutstandingCompilationRequests(true) && System->IsReadyToRun() && System->IsValid();
	Report += FString::Printf(TEXT("auditedReady=%d pendingCompile=%d readyToRun=%d systemValid=%d\n"),
		bReady, System->HasOutstandingCompilationRequests(true), System->IsReadyToRun(), System->IsValid());
	return bReady;
}

bool GatherCompactHit(UNiagaraSystem* System, TArray<FCompactHitValue>& Values,
	UNiagaraEmitter*& OutEmitter, TArray<UNiagaraScript*>& Scripts, FString& Report)
{
	if (!CanAccessNiagaraAsset(System, TEXT("User.CompactHitPreflight")) || System->GetPathName() != CompactHitPath)
		return CompactHitFailure(Report, TEXT("Exact editor-only candidate required, outside PIE"));
	const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
	if (Handles.Num() != 1 || !Handles[0].GetIsEnabled())
		return CompactHitFailure(Report, TEXT("Exactly one enabled stateful emitter required"));
	const FVersionedNiagaraEmitter Instance = Handles[0].GetInstance();
	if (!Instance.Emitter || !Instance.Version.IsValid() || Instance.Emitter->GetOutermost() != System->GetOutermost())
		return CompactHitFailure(Report, TEXT("Emitter must be the candidate-owned initialized instance"));
	FVersionedNiagaraEmitterData* Data = Instance.GetEmitterData();
	if (!Data || Data->SimTarget != ENiagaraSimTarget::CPUSim || Data->GetRenderers().Num() != 1 ||
		!IsValid(Data->GetRenderers()[0]) || !Data->GetRenderers()[0]->GetIsEnabled() ||
		!Data->GetRenderers()[0]->IsA<UNiagaraSpriteRendererProperties>() || Data->GetEventHandlers().Num() != 0)
		return CompactHitFailure(Report, TEXT("Exactly one CPU sprite renderer required; no ribbons"));
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Data->GraphSource);
	if (!Source || !Source->NodeGraph || Source->NodeGraph->Nodes.Num() > 128)
		return CompactHitFailure(Report, TEXT("Bounded initialized emitter graph required"));

	const TCHAR* ModulePaths[] = {
		TEXT("/Niagara/Modules/Spawn/Initialization/V2/InitializeParticle.InitializeParticle"),
		TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous"),
		TEXT("/Niagara/Modules/Emitter/EmitterState.EmitterState"),
		TEXT("/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState")};
	UNiagaraNodeFunctionCall* Modules[4] = {};
	for (UEdGraphNode* GraphNode : Source->NodeGraph->Nodes)
	{
		UNiagaraNodeFunctionCall* Node = Cast<UNiagaraNodeFunctionCall>(GraphNode);
		if (!Node || !Node->FunctionScript) continue;
		const FString FunctionPath = Node->FunctionScript->GetPathName();
		bool bAllowed = FunctionPath == TEXT("/Niagara/DynamicInputs/ValueFromCurve/FloatFromCurve.FloatFromCurve") ||
			FunctionPath == TEXT("/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity") ||
			FunctionPath == TEXT("/Niagara/Modules/Update/Color/ScaleColor.ScaleColor");
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (FunctionPath != ModulePaths[Index]) continue;
			bAllowed = true;
			if (Modules[Index]) return CompactHitFailure(Report, TEXT("Duplicate required module"));
			Modules[Index] = Node;
		}
		if (!bAllowed) return CompactHitFailure(Report, TEXT("Unexpected template function: ") + FunctionPath);
	}
	for (UNiagaraNodeFunctionCall* Module : Modules)
		if (!Module) return CompactHitFailure(Report, TEXT("Required SimpleSpriteBurst module missing"));
	if (!CompactHitEnumPin(Modules[2], TEXT("Life Cycle Mode"),
		TEXT("/Niagara/Enums/ENiagaraEmitterLifeCycleMode.ENiagaraEmitterLifeCycleMode"), TEXT("Self"), Report) ||
		!CompactHitEnumPin(Modules[2], TEXT("Loop Behavior"),
		TEXT("/Niagara/Enums/ENiagara_EmitterStateOptions.ENiagara_EmitterStateOptions"), TEXT("Once"), Report)) return false;
	if (!CompactHitBoolPin(Modules[3], TEXT("Kill Particles When Lifetime Has Elapsed"), true, Report) ||
		!CompactHitBoolPin(Modules[2], TEXT("Scale Spawn Count"), false, Report) ||
		!CompactHitBoolPin(Modules[2], TEXT("UseLoopDelay"), false, Report) ||
		!CompactHitBoolPin(Modules[1], TEXT("Use Spawn Probability"), false, Report) ||
		!CompactHitBoolPin(Modules[1], TEXT("UseLoopCountLimit"), false, Report)) return false;

	const FString Aliases[] = {
		Modules[0]->GetFunctionName() + TEXT(".Lifetime"),
		Modules[0]->GetFunctionName() + TEXT(".Uniform Sprite Size"),
		Modules[1]->GetFunctionName() + TEXT(".Spawn Count"),
		Modules[2]->GetFunctionName() + TEXT(".Loop Duration"),
		Modules[2]->GetFunctionName() + TEXT(".Loop Delay"),
		Modules[1]->GetFunctionName() + TEXT(".Spawn Time")};
	for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
	{
		if (!Node) return CompactHitFailure(Report, TEXT("Null graph node"));
		for (UEdGraphPin* Pin : Node->Pins)
			for (const FString& Alias : Aliases)
				if (Pin && Pin->PinName.ToString() == Alias)
					return CompactHitFailure(Report, TEXT("Graph override would take precedence over rapid iteration: ") + Alias);
	}

	if (!IsValid(System->GetSystemSpawnScript()) || !IsValid(System->GetSystemUpdateScript()))
		return CompactHitFailure(Report, TEXT("Factory-initialized system spawn/update scripts required"));
	Data->GetScripts(Scripts, false, false);
	Scripts.AddUnique(System->GetSystemSpawnScript());
	Scripts.AddUnique(System->GetSystemUpdateScript());
	if (Scripts.Num() > 16) return CompactHitFailure(Report, TEXT("Unexpected script count"));
	int32 Found[3] = {};
	bool bFiniteLoopDuration = false;
	bool bImmediateSpawn = false;
	for (UNiagaraScript* Script : Scripts)
	{
		if (!IsValid(Script) || Script->GetOutermost() != System->GetOutermost() ||
			Script->GetAllAvailableVersions().Num() == 0 || !Script->GetLatestSource())
			return CompactHitFailure(Report, TEXT("Candidate-owned initialized script version/source required"));
		for (const FNiagaraVariableWithOffset& Existing : Script->RapidIterationParameters.ReadParameterVariables())
		{
			const FString Name = Existing.GetName().ToString();
			if (!Name.StartsWith(TEXT("Constants."))) continue;
			if (Name.EndsWith(TEXT(".") + Aliases[5]))
			{
				if (Existing.GetType() != FNiagaraTypeDefinition::GetFloatDef() ||
					Script->RapidIterationParameters.GetParameterValue<float>(Existing) != 0.f)
					return CompactHitFailure(Report, TEXT("Existing Spawn Time must be float zero"));
				bImmediateSpawn = true;
			}
			if (Name.EndsWith(TEXT(".") + Modules[2]->GetFunctionName() + TEXT(".Loop Duration")))
			{
				if (Existing.GetType() != FNiagaraTypeDefinition::GetFloatDef())
					return CompactHitFailure(Report, TEXT("Loop Duration must be a float"));
				const float Duration = Script->RapidIterationParameters.GetParameterValue<float>(Existing);
				if (!FMath::IsFinite(Duration) || Duration <= 0.f || Duration > 10.f)
					return CompactHitFailure(Report, TEXT("Once loop duration must be finite in (0,10] seconds"));
				bFiniteLoopDuration = true;
				Report += FString::Printf(TEXT("loop=%s value=%.9g\n"), *Name, Duration);
			}
			for (int32 Target = 0; Target < 3; ++Target)
			{
				if (!Name.EndsWith(TEXT(".") + Aliases[Target])) continue;
				const FNiagaraTypeDefinition Type = Target == 2 ? FNiagaraTypeDefinition::GetIntDef() : FNiagaraTypeDefinition::GetFloatDef();
				const int32 ExpectedSize = Target == 2 ? sizeof(int32) : sizeof(float);
				if (Existing.GetType() != Type || Existing.GetSizeInBytes() != ExpectedSize)
					return CompactHitFailure(Report, TEXT("Exact scalar type/size mismatch: ") + Name);
				FCompactHitValue Value;
				Value.Script = Script;
				Value.Variable = FNiagaraVariable(Type, Existing.GetName());
				Value.Target = Target;
				if (Target == 2) Value.PreviousInt = Script->RapidIterationParameters.GetParameterValue<int32>(Existing);
				else
				{
					Value.PreviousFloat = Script->RapidIterationParameters.GetParameterValue<float>(Existing);
					if (!FMath::IsFinite(Value.PreviousFloat)) return CompactHitFailure(Report, TEXT("Nonfinite old value"));
				}
				Report += FString::Printf(TEXT("parameter script=%s name=%s type=%s old=%.9g target=%.9g\n"),
					*Script->GetPathName(), *Name, *Type.GetName(),
					Target == 2 ? double(Value.PreviousInt) : double(Value.PreviousFloat), Target == 2 ? 3.0 : Target == 0 ? .12 : 12.0);
				Values.Add(Value);
				++Found[Target];
			}
		}
	}
	if (!bFiniteLoopDuration || !bImmediateSpawn || Found[0] == 0 || Found[1] == 0 || Found[2] == 0)
		return CompactHitFailure(Report, TEXT("Required existing typed parameters or finite loop duration absent"));
	OutEmitter = Instance.Emitter;
	Report += FString::Printf(TEXT("preflight=1 renderer=Sprite counts=%d,%d,%d pendingCompile=%d ready=%d\n"),
		Found[0], Found[1], Found[2], System->HasOutstandingCompilationRequests(true), System->IsReadyToRun());
	for (UNiagaraScript* Script : Scripts)
		Report += FString::Printf(TEXT("compile script=%s status=%d\n"), *Script->GetPathName(), int32(Script->GetLastCompileStatus()));
	return true;
}

bool WriteCompactHitValue(const FCompactHitValue& Value, bool bRestore)
{
	if (Value.Target == 2)
		return Value.Script->RapidIterationParameters.SetParameterValue<int32>(bRestore ? Value.PreviousInt : 3, Value.Variable, false);
	return Value.Script->RapidIterationParameters.SetParameterValue<float>(
		bRestore ? Value.PreviousFloat : Value.Target == 0 ? .12f : 12.f, Value.Variable, false);
}
}
#endif

bool UBwayPresentationEditorLibrary::ConfigureCompactHitConfirm(UNiagaraSystem* System, bool bApply, FString& OutReport)
{
	OutReport = TEXT("scope=fixed-compact-hit-candidate saves=0 consumerBindings=0\n");
#if WITH_EDITOR
	TArray<FCompactHitValue> Values;
	TArray<UNiagaraScript*> Scripts;
	UNiagaraEmitter* Emitter = nullptr;
	if (!GatherCompactHit(System, Values, Emitter, Scripts, OutReport)) return false;
	bool bChanged = false;
	for (const FCompactHitValue& Value : Values)
		bChanged |= Value.Target == 2 ? Value.PreviousInt != 3 : Value.PreviousFloat != (Value.Target == 0 ? .12f : 12.f);
	OutReport += FString::Printf(TEXT("configured=%d\n"), !bChanged);
	const bool bReady = CompactHitReady(System, OutReport);
	if (!bApply) { OutReport += TEXT("dryRun=1 changed=0\n"); return bReady; }
	if (!bReady) return CompactHitFailure(OutReport, TEXT("A valid completed initial compile is required before applying"));
	if (!bChanged) { OutReport += TEXT("changed=0 alreadyConfigured=1\n"); return true; }
	System->Modify();
	Emitter->Modify();
	for (UNiagaraScript* Script : Scripts) Script->Modify();
	bool bWritten = true;
	for (const FCompactHitValue& Value : Values) bWritten &= WriteCompactHitValue(Value, false);
	for (const FCompactHitValue& Value : Values)
		bWritten &= Value.Target == 2 ? Value.Script->RapidIterationParameters.GetParameterValue<int32>(Value.Variable) == 3 :
			Value.Script->RapidIterationParameters.GetParameterValue<float>(Value.Variable) == (Value.Target == 0 ? .12f : 12.f);
	if (!bWritten)
	{
		bool bRestored = true;
		for (const FCompactHitValue& Value : Values) bRestored &= WriteCompactHitValue(Value, true);
		for (const FCompactHitValue& Value : Values)
			bRestored &= Value.Target == 2 ? Value.Script->RapidIterationParameters.GetParameterValue<int32>(Value.Variable) == Value.PreviousInt :
				Value.Script->RapidIterationParameters.GetParameterValue<float>(Value.Variable) == Value.PreviousFloat;
		OutReport += FString::Printf(TEXT("rollbackAttempted=1 rollbackReadbackVerified=%d\n"), bRestored);
	}
	for (UNiagaraScript* Script : Scripts) Script->PostEditChange();
	Emitter->PostEditChange();
	System->PostEditChange();
	System->MarkPackageDirty();
	const bool bRequested = System->RequestCompile(true);
	OutReport += FString::Printf(TEXT("changed=%d compileRequested=%d pendingCompile=%d saved=0 requireFreshAudit=1\n"),
		bWritten, bRequested, System->HasOutstandingCompilationRequests(true));
	return bWritten;
#else
	OutReport += TEXT("failure=Editor only\n");
	return false;
#endif
}

UNiagaraSystem* UBwayPresentationEditorLibrary::CreateCompactHitConfirmCandidate(FString& OutReport)
{
	OutReport = TEXT("scope=fixed-compact-hit-create saves=0 consumerBindings=0\n");
#if WITH_EDITOR
	if (!GIsEditor || !GEngine) { CompactHitFailure(OutReport, TEXT("Editor required")); return nullptr; }
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::PIE) { CompactHitFailure(OutReport, TEXT("Stop PIE first")); return nullptr; }
	const FString PackageName = TEXT("/BreakawayCore/Effects/Combat/NS_BW_HitConfirmCompact");
	if (FindPackage(nullptr, *PackageName) || FPackageName::DoesPackageExist(PackageName))
	{
		CompactHitFailure(OutReport, TEXT("Candidate package already exists in memory or on disk; no overwrite"));
		return nullptr;
	}
	FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("NiagaraEditor"));
	UNiagaraEmitter* Template = LoadObject<UNiagaraEmitter>(nullptr,
		TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst"));
	const FGuid Version = Template ? Template->GetExposedVersion().VersionGuid : FGuid();
	FVersionedNiagaraEmitterData* TemplateData = Template && Version.IsValid() ? Template->GetEmitterData(Version) : nullptr;
	if (!TemplateData || TemplateData->SimTarget != ENiagaraSimTarget::CPUSim || TemplateData->GetRenderers().Num() != 1 ||
		!IsValid(TemplateData->GetRenderers()[0]) || !TemplateData->GetRenderers()[0]->GetIsEnabled() ||
		!TemplateData->GetRenderers()[0]->IsA<UNiagaraSpriteRendererProperties>())
	{
		CompactHitFailure(OutReport, TEXT("Initialized CPU single-sprite SimpleSpriteBurst template required"));
		return nullptr;
	}
	UNiagaraScriptSource* TemplateSource = Cast<UNiagaraScriptSource>(TemplateData->GraphSource);
	if (!TemplateSource || !TemplateSource->NodeGraph || TemplateSource->NodeGraph->Nodes.Num() > 128)
	{
		CompactHitFailure(OutReport, TEXT("Bounded initialized template graph required"));
		return nullptr;
	}
	UNiagaraNodeFunctionCall* State = nullptr;
	for (UEdGraphNode* GraphNode : TemplateSource->NodeGraph->Nodes)
	{
		UNiagaraNodeFunctionCall* Node = Cast<UNiagaraNodeFunctionCall>(GraphNode);
		if (!Node || !Node->FunctionScript || Node->FunctionScript->GetPathName() !=
			TEXT("/Niagara/Modules/Emitter/EmitterState.EmitterState")) continue;
		if (State) { CompactHitFailure(OutReport, TEXT("Duplicate template EmitterState")); return nullptr; }
		State = Node;
	}
	if (!State || !CompactHitEnumPin(State, TEXT("Life Cycle Mode"),
		TEXT("/Niagara/Enums/ENiagaraEmitterLifeCycleMode.ENiagaraEmitterLifeCycleMode"), TEXT("Self"), OutReport) ||
		!CompactHitEnumPin(State, TEXT("Loop Behavior"),
		TEXT("/Niagara/Enums/ENiagara_EmitterStateOptions.ENiagara_EmitterStateOptions"), TEXT("Once"), OutReport))
	{
		CompactHitFailure(OutReport, TEXT("Template must have actual Self/Once loop settings"));
		return nullptr;
	}
	TArray<UNiagaraScript*> TemplateScripts;
	TemplateData->GetScripts(TemplateScripts, false, false);
	for (UNiagaraScript* Script : TemplateScripts)
		if (!IsValid(Script) || Script->GetAllAvailableVersions().Num() == 0 || !Script->GetLatestSource())
		{
			CompactHitFailure(OutReport, TEXT("Template script version/source missing"));
			return nullptr;
		}
	UClass* FactoryClass = LoadClass<UFactory>(nullptr, TEXT("/Script/NiagaraEditor.NiagaraSystemFactoryNew"));
	UFactory* Factory = FactoryClass ? NewObject<UFactory>(GetTransientPackage(), FactoryClass) : nullptr;
	if (!Factory) { CompactHitFailure(OutReport, TEXT("Exact Niagara system factory unavailable; no null-factory fallback")); return nullptr; }
	if (Factory->GetClass()->GetPathName() != TEXT("/Script/NiagaraEditor.NiagaraSystemFactoryNew"))
	{
		CompactHitFailure(OutReport, TEXT("Unexpected factory class"));
		return nullptr;
	}
	// Populate the exact factory's public native field before creation. Its own implementation
	// initializes system scripts, calls AddEmitterToSystem, then requests the first compile.
	UNiagaraSystemFactoryNew* NiagaraFactory = static_cast<UNiagaraSystemFactoryNew*>(Factory);
	FVersionedNiagaraEmitter VersionedTemplate;
	VersionedTemplate.Emitter = Template;
	VersionedTemplate.Version = Version;
	NiagaraFactory->EmittersToAddToNewSystem.Add(VersionedTemplate);
	FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UNiagaraSystem* System = Cast<UNiagaraSystem>(AssetTools.Get().CreateAsset(TEXT("NS_BW_HitConfirmCompact"),
		TEXT("/BreakawayCore/Effects/Combat"), UNiagaraSystem::StaticClass(), Factory));
	if (!System || System->GetPathName() != CompactHitPath)
	{
		CompactHitFailure(OutReport, TEXT("Factory creation failed; inspect any new unsaved candidate package"));
		return nullptr;
	}
	OutReport += TEXT("createdUnsaved=") + System->GetPathName() + TEXT("\n");
	for (UNiagaraScript* Script : {System->GetSystemSpawnScript(), System->GetSystemUpdateScript()})
		if (!IsValid(Script) || Script->GetAllAvailableVersions().Num() == 0 || !Script->GetLatestSource())
		{
			CompactHitFailure(OutReport, TEXT("Factory did not initialize system script/source/version; candidate remains unsaved"));
			return nullptr;
		}
	if (System->GetEmitterHandles().Num() != 1)
	{
		CompactHitFailure(OutReport, TEXT("Template copy failed; candidate remains unsaved"));
		return nullptr;
	}
	const FGuid HandleId = System->GetEmitterHandles()[0].GetId();
	if (!HandleId.IsValid())
	{
		CompactHitFailure(OutReport, TEXT("Invalid emitter handle; candidate remains unsaved"));
		return nullptr;
	}
	TArray<FCompactHitValue> Values;
	TArray<UNiagaraScript*> Scripts;
	UNiagaraEmitter* Emitter = nullptr;
	if (!GatherCompactHit(System, Values, Emitter, Scripts, OutReport))
	{
		OutReport += TEXT("candidateRemainsUnsaved=1\n");
		return nullptr;
	}
	System->MarkPackageDirty();
	const bool bRequested = System->HasOutstandingCompilationRequests(true) ? false : System->RequestCompile(true);
	OutReport += FString::Printf(TEXT("templateVersion=%s emitterHandle=%s compileRequested=%d pendingCompile=%d saved=0 requireFreshAudit=1\n"),
		*Version.ToString(), *HandleId.ToString(), bRequested, System->HasOutstandingCompilationRequests(true));
	return System;
#else
	OutReport += TEXT("failure=Editor only\n");
	return nullptr;
#endif
}
