#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Combat/BwayHeroProjectileBase.h"
#include "Combat/BwayProjectilePresentationData.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HAL/IConsoleManager.h"

namespace BwayProjectileDirectionTest
{
FTSTicker::FDelegateHandle Pending;

bool Run(float)
{
	Pending.Reset();
	UWorld* World = nullptr;
	int32 AuthorityCount = 0;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* Candidate = Context.World();
		if (Candidate && Candidate->WorldType == EWorldType::PIE && Candidate->GetNetMode() == NM_ListenServer && Candidate->HasBegunPlay())
		{
			World = Candidate;
			++AuthorityCount;
		}
	}
	if (AuthorityCount != 1 || GAllowActorScriptExecutionInEditor)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayProjectileDirection: SETUP_FAILED authorities=%d guard=%d"), AuthorityCount, GAllowActorScriptExecutionInEditor);
		return false;
	}

	const TCHAR* Families[] = {
		TEXT("/Script/HeroAlonaRuntime.BwayAlonaPrimaryProjectile"),
		TEXT("/Script/HeroMorganRuntime.BwayKorrynPrimaryProjectile"),
		TEXT("/Script/HeroRawlinsRuntime.BwayRawlinsBulletProjectile")
	};
	const FRotator Rotations[] = { FRotator::ZeroRotator, FRotator(0, 90, 0), FRotator(20, 37, 0), FRotator(-15, -135, 0) };
	int32 Cases = 0, Failures = 0, Destroyed = 0;
	for (const TCHAR* Family : Families)
	{
		UClass* Class = FindObject<UClass>(nullptr, Family);
		if (!Class || !Class->IsChildOf(ABwayHeroProjectileBase::StaticClass()))
		{
			++Failures;
			UE_LOG(LogTemp, Error, TEXT("BwayProjectileDirection: MISSING_CLASS class=%s"), Family);
			continue;
		}
		for (const FRotator& Rotation : Rotations)
		{
			for (int32 Route = 0; Route != 2; ++Route)
			{
				const FTransform Transform(Rotation, FVector(0, 0, 20000));
				ABwayHeroProjectileBase* Projectile = World->SpawnActorDeferred<ABwayHeroProjectileBase>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!Projectile) { ++Failures; continue; }
				Projectile->SetReplicates(false);
				Projectile->SetActorEnableCollision(false);
				UProjectileMovementComponent* Movement = Projectile->FindComponentByClass<UProjectileMovementComponent>();
				if (!Movement) { ++Failures; Destroyed += Projectile->Destroy(); continue; }
				// Exercise serialized local-space defaults before production configuration.
				Movement->bInitialVelocityInLocalSpace = true;
				if (Route == 0)
				{
					Projectile->ConfigureMovement(1234.f, 2.f);
				}
				else
				{
					Movement->Velocity = FVector::ZeroVector;
					UBwayProjectilePresentationData* Data = NewObject<UBwayProjectilePresentationData>(Projectile);
					Data->InitialSpeed = Data->MaxSpeed = 1234.f;
					Projectile->ApplyPresentation(Data);
				}
				Projectile->FinishSpawning(Transform);
				const FVector Velocity = Movement->Velocity;
				const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Velocity.GetSafeNormal(), Rotation.Vector()), -1.0, 1.0)));
				const bool Passed = FMath::IsFinite(Angle) && Angle < 0.01f && FMath::IsNearlyEqual(Velocity.Size(), 1234.0, 0.01);
				++Cases;
				Failures += !Passed;
				UE_LOG(LogTemp, Display, TEXT("BwayProjectileDirection: CASE class=%s route=%s pitch=%.6f yaw=%.6f vx=%.6f vy=%.6f vz=%.6f speed=%.6f angleError=%.6f localInitial=%d pass=%d guard=%d"),
					Family, Route == 0 ? TEXT("configured") : TEXT("presentation-fallback"), Rotation.Pitch, Rotation.Yaw,
					Velocity.X, Velocity.Y, Velocity.Z, Velocity.Size(), Angle, Movement->bInitialVelocityInLocalSpace, Passed, GAllowActorScriptExecutionInEditor);
				Destroyed += Projectile->Destroy();
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("BwayProjectileDirection: COMPLETE cases=%d failures=%d destroyed=%d pass=%d scope=authority-deferred-initialization-only"),
		Cases, Failures, Destroyed, Cases == 24 && Failures == 0 && Destroyed == 24);
	return false;
}

FAutoConsoleCommand Command(TEXT("bway.Test.ProjectileDirection"),
	TEXT("Development-only: verify deferred projectile directions across three native hero families; no collision, damage or transport proof."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		if (Pending.IsValid()) return;
		Pending = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Run));
	}));
}

#endif
