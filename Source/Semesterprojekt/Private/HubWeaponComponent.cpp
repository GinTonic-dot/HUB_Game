
#include "HubWeaponComponent.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

UHubWeaponComponent::UHubWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHubWeaponComponent::SetMuzzlePoints(
    USceneComponent* InLeftMuzzle,
    USceneComponent* InRightMuzzle)
{
    LeftMuzzle = InLeftMuzzle;
    RightMuzzle = InRightMuzzle;
}

void UHubWeaponComponent::SetAimTarget(FVector NewAimTarget)
{
    AimTarget = NewAimTarget;
    bHasAimTarget = true;
}

void UHubWeaponComponent::StartFire()
{
    if (bIsFiring || !GetWorld())
    {
        return;
    }

    if (!ProjectileClass || !LeftMuzzle || !RightMuzzle)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("HUB Weapon: Missing projectile or muzzle references."));
        return;
    }

    bIsFiring = true;

    // First shot immediately
    Fire();

    // Subsequent shots use a timer
    GetWorld()->GetTimerManager().SetTimer(
        FireTimerHandle,
        this,
        &UHubWeaponComponent::Fire,
        FMath::Max(FireInterval, 0.01f),
        true
    );
}

void UHubWeaponComponent::StopFire()
{
    bIsFiring = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FireTimerHandle);
    }
}

void UHubWeaponComponent::Fire()
{
    if (!bIsFiring || !GetWorld() || !ProjectileClass)
    {
        return;
    }

    USceneComponent* ActiveMuzzle =
        bFireLeftNext ? LeftMuzzle.Get() : RightMuzzle.Get();

    if (!IsValid(ActiveMuzzle))
    {
        StopFire();
        return;
    }

    // DEBUG
    UE_LOG(LogTemp, Warning,
        TEXT("HUB Weapon | Barrel: %s | Muzzle: %s | Location: %s"),
        bFireLeftNext ? TEXT("LEFT") : TEXT("RIGHT"),
        *ActiveMuzzle->GetName(),
        *ActiveMuzzle->GetComponentLocation().ToString()
    );
        
    const FVector SpawnLocation =
        ActiveMuzzle->GetComponentLocation();

    FVector FireDirection =
        ActiveMuzzle->GetForwardVector();

    if (bHasAimTarget)
    {
        const FVector ToTarget = AimTarget - SpawnLocation;

        if (!ToTarget.IsNearlyZero())
        {
            FireDirection = ToTarget.GetSafeNormal();
        }
    }

    const FRotator SpawnRotation = FireDirection.Rotation();

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.Instigator = Cast<APawn>(GetOwner());
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* SpawnedProjectile = GetWorld()->SpawnActor<AActor>(
        ProjectileClass,
        SpawnLocation,
        SpawnRotation,
        SpawnParams
    );

    if (!SpawnedProjectile)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("HUB Weapon: Projectile spawn failed."));
        return;
    }

    OnWeaponFired.Broadcast(ActiveMuzzle);

    // Switch barrel for next shot
    bFireLeftNext = !bFireLeftNext;
}

void UHubWeaponComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    StopFire();
    Super::EndPlay(EndPlayReason);
}
