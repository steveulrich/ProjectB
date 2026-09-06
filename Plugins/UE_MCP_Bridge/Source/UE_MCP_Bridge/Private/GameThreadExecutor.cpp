#include "GameThreadExecutor.h"
#include "HAL/PlatformProcess.h"
#include "Containers/Ticker.h"

namespace
{
struct FMCPExecutionState
{
	FMCPExecutionState() : DoneEvent(FPlatformProcess::GetSynchEventFromPool()) {}
	~FMCPExecutionState() { FPlatformProcess::ReturnSynchEventToPool(DoneEvent); }

	TSharedPtr<FJsonValue> Result;
	FEvent* DoneEvent;
	FThreadSafeBool bTimedOut{false};
};
}

FMCPGameThreadExecutor::FMCPGameThreadExecutor()
{
}

FMCPGameThreadExecutor::~FMCPGameThreadExecutor()
{
}

void FMCPGameThreadExecutor::SetEditorReady()
{
	bEditorReady = true;
}

bool FMCPGameThreadExecutor::IsGameThread()
{
	return IsInGameThread();
}

TSharedPtr<FJsonValue> FMCPGameThreadExecutor::ExecuteOnGameThread(FHandlerFunction Handler, const TSharedPtr<FJsonObject>& Params, float TimeoutSeconds)
{
	if (!bEditorReady)
	{
		TSharedPtr<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
		ErrorObject->SetStringField(TEXT("error"), TEXT("Editor is still initializing. Please wait and retry."));
		return MakeShared<FJsonValueObject>(ErrorObject);
	}

	if (IsGameThread())
	{
		// Already on game thread, execute directly
		return Handler(Params);
	}

	// Use FTSTicker to run on the game thread tick loop (NOT inside TaskGraph).
	// This avoids the TaskGraph recursion assertion when handlers trigger
	// subsystems like InterchangeEngine that schedule their own TaskGraph work.
	// The ticker can outlive this call after a timeout. It must own the inputs,
	// result, and event rather than retaining references to the caller's stack.
	const auto State = MakeShared<FMCPExecutionState, ESPMode::ThreadSafe>();

	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([State, Handler = MoveTemp(Handler), Params](float) -> bool
		{
			if (State->bTimedOut)
			{
				return false;
			}
			// Safety: verify GEditor and world are available before running handlers
			if (!GEditor || !GEditor->GetEditorWorldContext(false).World())
			{
				TSharedPtr<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
				ErrorObject->SetStringField(TEXT("error"), TEXT("Editor world not ready yet. Retry in a moment."));
				State->Result = MakeShared<FJsonValueObject>(ErrorObject);
				State->DoneEvent->Trigger();
				return false;
			}
			State->Result = Handler(Params);
			State->DoneEvent->Trigger();
			return false; // one-shot — do not re-tick
		})
	);

	// Block calling thread until the ticker fires or timeout
	uint32 TimeoutMs = static_cast<uint32>(TimeoutSeconds * 1000.0f);
	bool bCompleted = State->DoneEvent->Wait(TimeoutMs);

	if (!bCompleted)
	{
		State->bTimedOut = true;
		TSharedPtr<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
		ErrorObject->SetStringField(TEXT("error"), TEXT("Handler execution timed out"));
		return MakeShared<FJsonValueObject>(ErrorObject);
	}

	return State->Result;
}
