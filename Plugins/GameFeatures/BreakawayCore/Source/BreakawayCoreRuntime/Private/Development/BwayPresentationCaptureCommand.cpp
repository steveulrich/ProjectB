// Development evidence capture of one exact local player's Slate window, without desktop input.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "Camera/PlayerCameraManager.h"
#include "CoreGlobals.h"
#include "DynamicRHI.h"
#include "Development/BwayAllKitCombatProbe.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Layout/WidgetPath.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace BwayPresentationCapture
{
struct FRequest
{
	FString Token;
	FString Label;
	FString ImagePath;
	FString RecordPath;
	int32 PlayerId = INDEX_NONE;
	double QueuedAt = FPlatformTime::Seconds();
	int64 MaximumBytes = 32 * 1024 * 1024;
	int64 BytesWritten = 0;
	bool bFinished = false;
	bool bSaved = false;
	bool bPhaseMissed = false;
	int32 ExpectedCase = INDEX_NONE;
	int32 ExpectedStage = INDEX_NONE;
	double CaptureSeconds = 0;
};

TSharedPtr<FRequest> Active;
bool bFixtureCaptureActive = false;

bool IsSafeName(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 64) return false;
	for (TCHAR Character : Value)
	{
		if (!((Character >= TEXT('a') && Character <= TEXT('z'))
			|| (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('0') && Character <= TEXT('9'))
			|| Character == TEXT('-') || Character == TEXT('_'))) return false;
	}
	return true;
}

void Fail(const FRequest& Request, const TCHAR* Reason)
{
	UE_LOG(LogTemp, Error, TEXT("BwayPresentationCapture: FAIL token=%s player=%d label=%s reason=%s"),
		*Request.Token, Request.PlayerId, *Request.Label, Reason);
}

APlayerController* FindLocalPlayer(int32 PlayerId, int32& Matches)
{
	APlayerController* Selected = nullptr;
	Matches = 0;
	if (!GEngine) return nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || !World->IsGameWorld()) continue;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (IsValid(PC) && PC->IsLocalController() && PC->GetLocalPlayer() && PC->PlayerState
				&& PC->PlayerState->GetPlayerId() == PlayerId)
			{
				Selected = PC;
				++Matches;
			}
		}
	}
	return Matches == 1 ? Selected : nullptr;
}

const ABwayAllKitCombatProbe* FindAllKit(UWorld* World)
{
	const ABwayAllKitCombatProbe* Selected = nullptr;
	for (TActorIterator<ABwayAllKitCombatProbe> It(World); It; ++It)
	{
		if (It->IsActorBeingDestroyed()) continue;
		if (Selected) return nullptr; // Never attribute pixels to an ambiguous fixture.
		Selected = *It;
	}
	return Selected;
}

void Capture(FRequest& Request)
{
	if (!GEngine || !FSlateApplication::IsInitialized()) return Fail(Request, TEXT("engine-or-slate-unavailable"));
	if (!GDynamicRHI || FString(GDynamicRHI->GetName()).Contains(TEXT("Null"))) return Fail(Request, TEXT("rendered-rhi-required"));
	if (GIsHighResScreenshot || FScreenshotRequest::IsScreenshotRequested()) return Fail(Request, TEXT("another-screenshot-pending"));

	// PlayerState IDs also appear on replicated remote controllers. Only an actual
	// local player with its own viewport is eligible; ambiguous matches fail closed.
	int32 Matches = 0;
	APlayerController* Selected = FindLocalPlayer(Request.PlayerId, Matches);
	if (Matches != 1) return Fail(Request, Matches ? TEXT("ambiguous-local-player-id") : TEXT("local-player-id-not-found"));

	UWorld* World = Selected->GetWorld();
	UGameViewportClient* Client = Selected->GetLocalPlayer()->ViewportClient;
	if (!Client || Client->GetWorld() != World || !Client->Viewport) return Fail(Request, TEXT("local-viewport-world-mismatch"));
	const TSharedPtr<SViewport> ViewWidget = Client->GetGameViewportWidget();
	const TSharedPtr<SWindow> Window = Client->GetWindow();
	if (!ViewWidget.IsValid() || !Window.IsValid()) return Fail(Request, TEXT("local-viewport-window-unavailable"));
	const FVector2D WindowSize = Window->GetSizeInScreen();
	if (WindowSize.X <= 0 || WindowSize.Y <= 0 || WindowSize.X * WindowSize.Y > 9000000)
		return Fail(Request, TEXT("window-exceeds-nine-megapixel-capture-bound"));
	FSlateApplication& Slate = FSlateApplication::Get();
	if (Slate.FindWidgetWindow(ViewWidget.ToSharedRef()) != Window) return Fail(Request, TEXT("viewport-widget-window-mismatch"));
	FWidgetPath ViewPath;
	if (!Slate.GeneratePathToWidgetUnchecked(ViewWidget.ToSharedRef(), ViewPath)) return Fail(Request, TEXT("selected-viewport-not-in-visible-slate-tree"));
	if (IFileManager::Get().FileExists(*Request.ImagePath) || IFileManager::Get().FileExists(*Request.RecordPath))
		return Fail(Request, TEXT("evidence-path-already-exists"));
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Request.ImagePath), true)) return Fail(Request, TEXT("evidence-directory-unavailable"));

	const FIntPoint ViewportSize = Client->Viewport->GetSizeXY();
	const FString WorldPath = World->GetPathName();
	const FString ControllerPath = Selected->GetPathName();
	const FString ViewportPath = Client->GetPathName();
	const FString PawnPath = GetPathNameSafe(Selected->GetPawn());
	const FString ViewTargetPath = GetPathNameSafe(Selected->GetViewTarget());
	const APlayerCameraManager* Camera = Selected->PlayerCameraManager;
	const FVector CameraLocation = Camera ? Camera->GetCameraLocation() : FVector::ZeroVector;
	const FRotator CameraRotation = Camera ? Camera->GetCameraRotation() : FRotator::ZeroRotator;
	const float FieldOfView = Camera ? Camera->GetFOVAngle() : 0.f;
	const FString WindowTitle = Window->GetTitle().ToString();
	const FString RHIName = GDynamicRHI->GetName();
	const int32 NetMode = int32(World->GetNetMode());
	const double WorldSeconds = World->GetTimeSeconds();
	const uint64 Frame = GFrameCounter;
	const ABwayAllKitCombatProbe* Probe = FindAllKit(World);
	const FBwayAllKitCombatFrame Fixture = Probe ? Probe->GetFrame() : FBwayAllKitCombatFrame();
	if (Request.ExpectedCase != INDEX_NONE && (!Probe || Fixture.Control != 0
		|| Fixture.Case != Request.ExpectedCase || Fixture.Stage != Request.ExpectedStage))
	{
		Request.bPhaseMissed = true;
		UE_LOG(LogTemp, Warning, TEXT("BwayPresentationCapture: Missed token=%s player=%d label=%s expectedCase=%d expectedStage=%d actualCase=%d actualStage=%d"),
			*Request.Token, Request.PlayerId, *Request.Label, Request.ExpectedCase, Request.ExpectedStage, Fixture.Case, Fixture.Stage);
		return;
	}
	const double CaptureStarted = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Display, TEXT("BwayPresentationCapture: Selected token=%s player=%d label=%s world=%s controller=%s viewport=%s net=%d local=1 rhi=%s viewportSize=%dx%d guard=%d"),
		*Request.Token, Request.PlayerId, *Request.Label, *WorldPath, *ControllerPath, *ViewportPath,
		NetMode, *RHIName, ViewportSize.X, ViewportSize.Y, GAllowActorScriptExecutionInEditor);

	// Refresh only the selected game view, then let Slate render and read back its
	// exact window. No OS capture, foreground switch, global screenshot request,
	// first-world fallback, synthetic input, or gameplay activation is involved.
	Client->Viewport->Draw(false);
	TArray<FColor> Pixels;
	FIntVector ImageSize = FIntVector::ZeroValue;
	if (!Slate.TakeScreenshot(Window.ToSharedRef(), Pixels, ImageSize)
		|| ImageSize.X <= 0 || ImageSize.Y <= 0 || int64(ImageSize.X) * ImageSize.Y != Pixels.Num())
	{
		return Fail(Request, TEXT("exact-window-render-readback-failed"));
	}
	for (FColor& Pixel : Pixels) Pixel.A = 255;
	TArray64<uint8> PNG;
	FImageUtils::PNGCompressImageArray(ImageSize.X, ImageSize.Y,
		TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), PNG);
	if (PNG.Num() > Request.MaximumBytes) return Fail(Request, TEXT("png-exceeds-session-byte-budget"));
	if (PNG.IsEmpty() || !FFileHelper::SaveArrayToFile(PNG, *Request.ImagePath,
		&IFileManager::Get(), FILEWRITE_NoReplaceExisting)) return Fail(Request, TEXT("png-write-failed"));
	Request.BytesWritten = PNG.Num();
	Request.CaptureSeconds = FPlatformTime::Seconds() - CaptureStarted;

	const auto Quote = [](const FString& Value) { return FString::Printf(TEXT("\"%s\""), *Value.ReplaceCharWithEscapedChar()); };
	const FString Record = FString::Printf(
		TEXT("{\n  \"schemaVersion\":1,\n  \"token\":%s,\n  \"label\":%s,\n  \"playerId\":%d,\n")
		TEXT("  \"capturedUtc\":%s,\n  \"world\":%s,\n  \"controller\":%s,\n  \"pawn\":%s,\n  \"viewport\":%s,\n  \"windowTitle\":%s,\n")
		TEXT("  \"netMode\":%d,\n  \"localPlayer\":true,\n  \"rhi\":%s,\n  \"frame\":%llu,\n  \"worldSeconds\":%.6f,\n")
		TEXT("  \"viewTarget\":%s,\n  \"fieldOfView\":%.4f,\n  \"cameraLocation\":[%.4f,%.4f,%.4f],\n  \"cameraRotation\":[%.4f,%.4f,%.4f],\n")
		TEXT("  \"viewportWidth\":%d,\n  \"viewportHeight\":%d,\n  \"imageWidth\":%d,\n  \"imageHeight\":%d,\n  \"png\":%s,\n")
		TEXT("  \"allKitPresent\":%s,\n  \"allKitCase\":%d,\n  \"allKitControl\":%d,\n  \"allKitStage\":%d,\n  \"allKitSerial\":%d,\n  \"allKitSource\":%d,\n  \"allKitAction\":%s,\n  \"observerIsActionOwner\":%s,\n")
		TEXT("  \"captureSeconds\":%.6f,\n  \"pngBytes\":%lld,\n")
		TEXT("  \"scope\":\"Exact local-player Slate window including HUD; embedded PIE may include editor chrome. Pixel content still requires review.\",\n")
		TEXT("  \"desktopInputUsed\":false,\n  \"physicalInputVerified\":false,\n  \"visualAcceptancePassed\":false\n}\n"),
		*Quote(Request.Token), *Quote(Request.Label), Request.PlayerId, *Quote(FDateTime::UtcNow().ToIso8601()),
		*Quote(WorldPath), *Quote(ControllerPath), *Quote(PawnPath), *Quote(ViewportPath), *Quote(WindowTitle),
		NetMode, *Quote(RHIName), Frame, WorldSeconds, *Quote(ViewTargetPath), FieldOfView,
		CameraLocation.X, CameraLocation.Y, CameraLocation.Z, CameraRotation.Pitch, CameraRotation.Yaw, CameraRotation.Roll,
		ViewportSize.X, ViewportSize.Y,
		ImageSize.X, ImageSize.Y, *Quote(Request.ImagePath), Probe ? TEXT("true") : TEXT("false"),
		Fixture.Case, Fixture.Control, Fixture.Stage, Fixture.Serial, Fixture.Source, *Quote(Fixture.Action.ToString()),
		Probe && Fixture.Source == Request.PlayerId ? TEXT("true") : TEXT("false"),
		Request.CaptureSeconds, Request.BytesWritten);
	if (!FFileHelper::SaveStringToFile(Record, *Request.RecordPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), FILEWRITE_NoReplaceExisting)) return Fail(Request, TEXT("metadata-write-failed-png-preserved"));
	Request.bSaved = true;
	UE_LOG(LogTemp, Display, TEXT("BwayPresentationCapture: Saved token=%s player=%d label=%s world=%s net=%d local=1 rhi=%s image=%dx%d frame=%llu guard=%d png=%s record=%s visualAcceptance=0"),
		*Request.Token, Request.PlayerId, *Request.Label, *WorldPath, NetMode, *RHIName,
		ImageSize.X, ImageSize.Y, Frame, GAllowActorScriptExecutionInEditor, *Request.ImagePath, *Request.RecordPath);
}

TSharedPtr<FRequest> Queue(const FString& Token, int32 PlayerId, const FString& Label, int64 MaximumBytes = 32 * 1024 * 1024)
{
	if (Active.IsValid()) return nullptr;
	Active = MakeShared<FRequest>();
	Active->Token = Token; Active->PlayerId = PlayerId; Active->Label = Label; Active->MaximumBytes = MaximumBytes;
	const FString BaseName = FString::Printf(TEXT("player%d-%s"), PlayerId, *Label);
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs/PresentationCaptures"), Token);
	Active->ImagePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(Directory, BaseName + TEXT(".png")));
	Active->RecordPath = FPaths::ChangeExtension(Active->ImagePath, TEXT("json"));
	const TSharedPtr<FRequest> Request = Active;
	UE_LOG(LogTemp, Display, TEXT("BwayPresentationCapture: Queued token=%s player=%d label=%s"), *Token, PlayerId, *Label);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Request](float)
	{
		if (Active != Request) return false;
		if (FPlatformTime::Seconds() - Request->QueuedAt > 5.0)
		{
			Fail(*Request, TEXT("native-dispatch-timeout")); Request->bFinished = true; Active.Reset(); return false;
		}
		if (GAllowActorScriptExecutionInEditor) return true;
		Capture(*Request);
		Request->bFinished = true;
		Active.Reset();
		return false;
	}), 0.01f);
	return Request;
}

FAutoConsoleCommand Command(TEXT("bway.Test.CapturePresentation"),
	TEXT("Development: <fresh-token> <local-player-id> <label>. Capture only the selected local player's Slate window including HUD; no desktop input or implicit visual pass."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		int32 PlayerId = INDEX_NONE;
		if (Args.Num() != 3 || !IsSafeName(Args[0]) || !LexTryParseString(PlayerId, *Args[1])
			|| PlayerId < 0 || !IsSafeName(Args[2]))
		{
			UE_LOG(LogTemp, Error, TEXT("BwayPresentationCapture: FAIL reason=usage expected=<token> <local-player-id> <label> names=ASCII-alphanumeric-hyphen-underscore-max64"));
			return;
		}
		if (Active.IsValid() || bFixtureCaptureActive)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayPresentationCapture: FAIL token=%s reason=another-capture-or-session-pending"), *Args[0]);
			return;
		}
		Queue(Args[0], PlayerId, Args[2]);
	}));

struct FFixtureView
{
	int32 PlayerId = INDEX_NONE;
	int32 CurrentCase = INDEX_NONE;
	int32 Sample = 0;
	double EnteredActiveAt = 0;
	bool bOutcomeQueued = false;
	bool bSawFixture = false;
	bool bDone = false;
	TSet<int32> ActiveCases;
	TSet<int32> OutcomeCases;
};

struct FFixtureSession
{
	FString Token;
	FString Label;
	TArray<FFixtureView> Views;
	TSharedPtr<FRequest> Pending;
	int32 NextView = 0;
	int32 Count = 0;
	int32 Saved = 0;
	int32 Missed = 0;
	int32 SequenceCount = 0; // Zero selects the AllKit frame observer.
	double Interval = .2;
	double NextAt = 0;
	double Started = FPlatformTime::Seconds();
	int64 Bytes = 0;
	static constexpr int64 MaximumBytes = 512ll * 1024 * 1024;
};

void FinishSession(const FFixtureSession& Session, const TCHAR* Reason)
{
	UE_LOG(LogTemp, Display, TEXT("BwayPresentationCapture: SessionFinished token=%s reason=%s queued=%d saved=%d phaseMisses=%d bytes=%lld visualAcceptance=0 physicalInput=0"),
		*Session.Token, Reason, Session.Count, Session.Saved, Session.Missed, Session.Bytes);
	for (const FFixtureView& View : Session.Views)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayPresentationCapture: Coverage token=%s observer=%d activeCases=%d outcomeCases=%d expectedCases=%d visualAcceptance=0"),
			*Session.Token, View.PlayerId, View.ActiveCases.Num(), View.OutcomeCases.Num(), Session.SequenceCount ? 0 : 20);
	}
	bFixtureCaptureActive = false;
}

void StartSession(const TSharedPtr<FFixtureSession>& Session)
{
	bFixtureCaptureActive = true;
	UE_LOG(LogTemp, Display, TEXT("BwayPresentationCapture: SessionStarted token=%s mode=%s observers=%d maximumBytes=%lld"),
		*Session->Token, Session->SequenceCount ? TEXT("sequence") : TEXT("AllKit"), Session->Views.Num(), FFixtureSession::MaximumBytes);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Session](float)
	{
		const double Now = FPlatformTime::Seconds();
		if (Session->Pending)
		{
			if (!Session->Pending->bFinished) return true;
			const FRequest& Request = *Session->Pending;
			Session->Bytes += Request.BytesWritten;
			if (Request.bSaved)
			{
				++Session->Saved;
				for (FFixtureView& View : Session->Views) if (View.PlayerId == Request.PlayerId)
				{
					if (Request.ExpectedStage == 2) View.ActiveCases.Add(Request.ExpectedCase);
					if (Request.ExpectedStage == 3) View.OutcomeCases.Add(Request.ExpectedCase);
				}
			}
			else if (Request.bPhaseMissed) ++Session->Missed;
			else { FinishSession(*Session, TEXT("capture-failed")); return false; }
			if (Request.CaptureSeconds > 2.0) { FinishSession(*Session, TEXT("render-readback-exceeded-two-second-stall-bound")); return false; }
			Session->Pending.Reset();
		}
		if (Now - Session->Started > (Session->SequenceCount ? 35.0 : 450.0))
		{
			FinishSession(*Session, TEXT("session-timeout")); return false;
		}
		if (Session->Bytes >= FFixtureSession::MaximumBytes || Session->Count >= 160)
		{
			FinishSession(*Session, TEXT("session-budget-reached")); return false;
		}
		if (GAllowActorScriptExecutionInEditor || Active.IsValid()) return true;
		if (Session->SequenceCount)
		{
			if (Session->Count >= Session->SequenceCount) { FinishSession(*Session, TEXT("sequence-finished")); return false; }
			if (Now < Session->NextAt) return true;
			const int32 ViewIndex = Session->Count % Session->Views.Num();
			Session->Pending = Queue(Session->Token, Session->Views[ViewIndex].PlayerId,
				FString::Printf(TEXT("%s-%03d"), *Session->Label, Session->Count / Session->Views.Num()),
				FMath::Min<int64>(32ll * 1024 * 1024, FFixtureSession::MaximumBytes - Session->Bytes));
			++Session->Count;
			Session->NextAt = ViewIndex + 1 == Session->Views.Num() ? Now + Session->Interval : Now;
			return true;
		}
		bool bAllDone = true;
		for (int32 Offset = 0; Offset < Session->Views.Num(); ++Offset)
		{
			const int32 Index = (Session->NextView + Offset) % Session->Views.Num();
			FFixtureView& View = Session->Views[Index];
			if (View.bDone) continue;
			bAllDone = false;
			int32 Matches = 0;
			APlayerController* PC = FindLocalPlayer(View.PlayerId, Matches);
			if (!PC) { FinishSession(*Session, TEXT("selected-local-player-lost-or-ambiguous")); return false; }
			const ABwayAllKitCombatProbe* Probe = FindAllKit(PC->GetWorld());
			if (!Probe)
			{
				if (View.bSawFixture || Now - Session->Started > 30.0) { FinishSession(*Session, TEXT("fixture-missing-or-ambiguous")); return false; }
				continue;
			}
			View.bSawFixture = true;
			const FBwayAllKitCombatFrame& Frame = Probe->GetFrame();
			if (Frame.Stage == 9) { FinishSession(*Session, TEXT("fixture-failed")); return false; }
			if (Frame.Case >= 20) { View.bDone = true; continue; }
			if (Frame.Control != 0 || Frame.Case < 0) continue;
			if (View.CurrentCase != Frame.Case)
			{
				View.CurrentCase = Frame.Case; View.Sample = 0; View.EnteredActiveAt = 0; View.bOutcomeQueued = false;
			}
			FString Label;
			if (Frame.Stage == 2 && View.Sample < 3)
			{
				if (View.EnteredActiveAt == 0) View.EnteredActiveAt = Now;
				const double Offsets[] = {.10, .35, FMath::Clamp(double(Frame.Duration) * .8, .40, 1.10)};
				if (Now - View.EnteredActiveAt < Offsets[View.Sample]) continue;
				Label = FString::Printf(TEXT("allkit-case%02d-active%d"), Frame.Case, View.Sample++);
			}
			else if (Frame.Stage == 3 && !View.bOutcomeQueued)
			{
				View.bOutcomeQueued = true;
				Label = FString::Printf(TEXT("allkit-case%02d-outcome"), Frame.Case);
			}
			else continue;
			Session->Pending = Queue(Session->Token, View.PlayerId, Label,
				FMath::Min<int64>(32ll * 1024 * 1024, FFixtureSession::MaximumBytes - Session->Bytes));
			Session->Pending->ExpectedCase = Frame.Case;
			Session->Pending->ExpectedStage = Frame.Stage;
			++Session->Count;
			Session->NextView = (Index + 1) % Session->Views.Num();
			return true;
		}
		if (bAllDone) { FinishSession(*Session, TEXT("allkit-observation-finished")); return false; }
		return true;
	}), .01f);
}

bool ParseView(const FString& Value, FFixtureView& View)
{
	int32 Matches = 0;
	return LexTryParseString(View.PlayerId, *Value) && View.PlayerId >= 0 && FindLocalPlayer(View.PlayerId, Matches);
}

FAutoConsoleCommand AllKitCommand(TEXT("bway.Test.CaptureAllKit"),
	TEXT("Development: <fresh-token> <local-player-id> [second-local-player-id]. Arm before AllKitOwnerHost; observe three active offsets plus outcome for all 20 positive cases, at most two exact Slate windows."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2 || Args.Num() > 3 || !IsSafeName(Args[0]) || Active.IsValid() || bFixtureCaptureActive)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayPresentationCapture: FAIL reason=AllKit-usage-or-capture-busy")); return;
		}
		auto Session = MakeShared<FFixtureSession>();
		Session->Token = Args[0];
		for (int32 I = 1; I < Args.Num(); ++I)
		{
			FFixtureView View;
			if (!ParseView(Args[I], View) || (I == 2 && View.PlayerId == Session->Views[0].PlayerId))
			{
				UE_LOG(LogTemp, Error, TEXT("BwayPresentationCapture: FAIL reason=unique-rendered-local-player-ids-required")); return;
			}
			Session->Views.Add(View);
		}
		StartSession(Session);
	}));

FAutoConsoleCommand SequenceCommand(TEXT("bway.Test.CapturePresentationSequence"),
	TEXT("Development: <fresh-token> <local-player-id> <label> <count-per-view 2..40> <interval .05..2 seconds> [second-local-player-id]. Arm before a fixture; at most 30 seconds of scheduled samples. Does not activate gameplay."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		auto Session = MakeShared<FFixtureSession>();
		FFixtureView View;
		if ((Args.Num() != 5 && Args.Num() != 6) || !IsSafeName(Args[0]) || !IsSafeName(Args[2]) || Args[2].Len() > 56
			|| !ParseView(Args[1], View) || !LexTryParseString(Session->SequenceCount, *Args[3])
			|| !LexTryParseString(Session->Interval, *Args[4]) || !FMath::IsFinite(Session->Interval)
			|| Session->SequenceCount < 2 || Session->SequenceCount > 40 || Session->Interval < .05 || Session->Interval > 2
			|| Session->SequenceCount * Session->Interval > 30 || Active.IsValid() || bFixtureCaptureActive)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayPresentationCapture: FAIL reason=sequence-usage-or-capture-busy")); return;
		}
		Session->Token = Args[0]; Session->Label = Args[2]; Session->Views.Add(View);
		if (Args.Num() == 6)
		{
			FFixtureView SecondView;
			if (!ParseView(Args[5], SecondView) || SecondView.PlayerId == View.PlayerId)
			{
				UE_LOG(LogTemp, Error, TEXT("BwayPresentationCapture: FAIL reason=unique-second-local-player-required")); return;
			}
			Session->Views.Add(SecondView);
			Session->SequenceCount *= 2;
		}
		StartSession(Session);
	}));
}

#endif
