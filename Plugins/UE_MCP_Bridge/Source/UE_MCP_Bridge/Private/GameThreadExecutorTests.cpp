#include "GameThreadExecutor.h"
#include "Async/Async.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FVerifyTimedOutHandlerSkipped : public IAutomationLatentCommand
{
public:
	FVerifyTimedOutHandlerSkipped(FAutomationTestBase* InTest, TSharedRef<FThreadSafeCounter, ESPMode::ThreadSafe> InCalls)
		: Test(InTest), Calls(InCalls) {}
	virtual bool Update() override
	{
		Test->TestEqual(TEXT("Expired queued handler never executes on a later tick"), Calls->GetValue(), 0);
		return true;
	}
private:
	FAutomationTestBase* Test;
	TSharedRef<FThreadSafeCounter, ESPMode::ThreadSafe> Calls;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPQueuedTimeoutTest, "ProjectB.MCP.QueuedTimeoutLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMCPQueuedTimeoutTest::RunTest(const FString& Parameters)
{
	const auto Calls = MakeShared<FThreadSafeCounter, ESPMode::ThreadSafe>();
	// While this game-thread test waits, the worker must time out before the
	// ticker can run. Its executor, parameters, and function stack then expire.
	auto Future = Async(EAsyncExecution::Thread, [Calls]()
	{
		FMCPGameThreadExecutor Executor;
		Executor.SetEditorReady();
		return Executor.ExecuteOnGameThread([Calls](const TSharedPtr<FJsonObject>&)
		{
			Calls->Increment();
			return MakeShared<FJsonValueNull>();
		}, MakeShared<FJsonObject>(), 0.001f);
	});
	const auto Result = Future.Get();
	FString Error;
	TestTrue(TEXT("Worker receives timeout"), Result.IsValid()
		&& Result->AsObject()->TryGetStringField(TEXT("error"), Error)
		&& Error == TEXT("Handler execution timed out"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.1f));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FVerifyTimedOutHandlerSkipped>(this, Calls));
	return true;
}
#endif
