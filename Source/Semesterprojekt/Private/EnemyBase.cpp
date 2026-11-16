#include "EnemyBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/World.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"


AEnemyBase::AEnemyBase()
{
    PrimaryActorTick.bCanEverTick = true;

    // Collision als Root
    CollisionCapsule =
        CreateDefaultSubobject<UCapsuleComponent>(
            TEXT("CollisionCapsule")
        );

    RootComponent = CollisionCapsule;

    CollisionCapsule->InitCapsuleSize(42.0f, 88.0f);

    CollisionCapsule->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly
    );

    CollisionCapsule->SetCollisionObjectType(ECC_Pawn);

    CollisionCapsule->SetCollisionResponseToAllChannels(
        ECR_Block
    );


    // Mesh genau einmal erstellen
    EnemyMesh =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("EnemyMesh")
        );

    EnemyMesh->SetupAttachment(CollisionCapsule);

    EnemyMesh->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
    
    // Gegner blockieren einander nicht, bleiben aber fuer Query-Overlaps erkennbar.
    CollisionCapsule->SetCollisionResponseToChannel(
        ECC_Pawn,
        ECR_Overlap
    );

    // Health Component
    HealthComponent =
        CreateDefaultSubobject<UHealthComponent>(
            TEXT("HealthComponent")
        );
}


void AEnemyBase::BeginPlay()
{
    Super::BeginPlay();

    // Death-Event beibehalten
    if (HealthComponent)
    {
        HealthComponent->OnDeath.AddDynamic(
            this,
            &AEnemyBase::HandleDeath
        );
    }

    // Wegsuche zeitlich versetzt starten,
    // damit nicht alle Gegner gleichzeitig rechnen.
    const float SafeInterval =
        FMath::Max(0.2f, RepathInterval);

    const float InitialDelay =
        FMath::FRandRange(0.05f, SafeInterval);

    GetWorldTimerManager().SetTimer(
        RepathTimerHandle,
        this,
        &AEnemyBase::RefreshNavigationPath,
        SafeInterval,
        true,
        InitialDelay
    );
}


void AEnemyBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateState();

    switch (CurrentState)
    {
    case EEnemyState::Moving:
        MoveAlongNavigationPath(DeltaTime);
        break;
        
    case EEnemyState::Attacking:
        MoveDuringAttack(DeltaTime);
        break;

    case EEnemyState::Stunned:
        break;

    case EEnemyState::Dead:
        break;
    }
    
    
    if (CurrentState == EEnemyState::Moving || CurrentState == EEnemyState::Attacking)
    {
        SeparationUpdateElapsed += DeltaTime;

        if (SeparationUpdateElapsed >= 0.15f)
        {
            SeparationUpdateElapsed = 0.0f;
            UpdateSeparation();
        }
    }

}



void AEnemyBase::UpdateState()
{
    if (CurrentState == EEnemyState::Dead ||
        CurrentState == EEnemyState::Stunned)
    {
        return;
    }

    FVector TargetLocation;
    float Distance;

    if (!GetHubTargetData(TargetLocation, Distance))
    {
        SetEnemyState(EEnemyState::Moving);
        return;
    }

    if (Distance <= AttackRange)
    {
        SetEnemyState(EEnemyState::Attacking);
    }
    else
    {
        SetEnemyState(EEnemyState::Moving);
    }
}



void AEnemyBase::SetEnemyState(EEnemyState NewState)
{
    if (CurrentState == NewState)
    {
        return;
    }

    // Alten Angriff beenden
    if (CurrentState == EEnemyState::Attacking)
    {
        StopAttacking();
    }

    CurrentState = NewState;

    UE_LOG(LogTemp, Log,
        TEXT("Enemy State changed to: %s"),
        *UEnum::GetValueAsString(CurrentState));

    // Neuen Angriff starten
    if (CurrentState == EEnemyState::Attacking)
    {
        StartAttacking();
    }
}

void AEnemyBase::HandleAttack()
{
    UE_LOG(LogTemp, Warning,
        TEXT("HANDLE ATTACK CALLED"));

    if (CurrentState != EEnemyState::Attacking ||
        !IsValid(TargetHub))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Attack failed: Invalid State or Target"));
        return;
    }

    FVector TargetLocation;
    float Distance;

    if (!GetHubTargetData(TargetLocation, Distance))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Attack failed: GetHubTargetData"));
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("Attack Distance: %.1f | Range: %.1f"),
        Distance,
        AttackRange);

    if (Distance > AttackRange)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Attack failed: Out of Range"));
        return;
    }

    const float ActualDamage = UGameplayStatics::ApplyDamage(
        TargetHub,
        AttackDamage,
        nullptr,
        this,
        UDamageType::StaticClass()
    );

    UE_LOG(LogTemp, Warning,
        TEXT("ApplyDamage: %.1f | Target: %s | CanBeDamaged: %s"),
        ActualDamage,
        *GetNameSafe(TargetHub),
        TargetHub->CanBeDamaged() ? TEXT("TRUE") : TEXT("FALSE")
    );
}



void AEnemyBase::HandleDeath()
{
    // Schutz vor mehrfacher Todesverarbeitung.
    if (CurrentState == EEnemyState::Dead)
    {
        return;
    }

    // 1. State Machine auf Dead setzen.
    // SetEnemyState stoppt bereits den Angriffstimer,
    // wenn der Gegner vorher angegriffen hat.
    SetEnemyState(EEnemyState::Dead);

    // 2. Sicherheitshalber laufende Timer stoppen.
    GetWorldTimerManager().ClearTimer(AttackTimerHandle);
    GetWorldTimerManager().ClearTimer(RepathTimerHandle);

    // 3. Navigation und Separation deaktivieren.
    bHasValidPath = false;
    NavigationPoints.Reset();
    CachedSeparation = FVector::ZeroVector;

    // 4. Kollision deaktivieren.
    // Tote Gegner sollen keine Projektile blockieren
    // und nicht mehr gegen den HUB stoßen.
    if (IsValid(CollisionCapsule))
    {
        CollisionCapsule->SetCollisionEnabled(
            ECollisionEnabled::NoCollision
        );
    }

    // 5. Blueprint-Event für Animation, VFX und Sound.
    OnEnemyDeathVisuals();

    UE_LOG(LogTemp, Log,
        TEXT("Enemy died: %s"), *GetName());

    // 6. Actor nach 5 Sekunden entfernen.
    // Genug Zeit für spätere Death-Animationen.
    SetLifeSpan(5.0f);
}


void AEnemyBase::StartAttacking()
{
    UE_LOG(LogTemp, Warning,
        TEXT("START ATTACKING CALLED"));

    if (!IsValid(TargetHub))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Attack failed: Invalid TargetHub"));
        return;
    }

    const float SafeCooldown =
        FMath::Max(0.1f, AttackCooldown);

    GetWorldTimerManager().SetTimer(
        AttackTimerHandle,
        this,
        &AEnemyBase::HandleAttack,
        SafeCooldown,
        true
    );

    UE_LOG(LogTemp, Warning,
        TEXT("Attack Timer Active: %s"),
        GetWorldTimerManager().IsTimerActive(AttackTimerHandle)
            ? TEXT("TRUE") : TEXT("FALSE"));
}
void AEnemyBase::StopAttacking()
{
    GetWorldTimerManager().ClearTimer(
        AttackTimerHandle
    );
}



bool AEnemyBase::GetHubTargetData(
    FVector& OutTargetLocation,
    float& OutDistance
) const
{
    if (!IsValid(TargetHub))
    {
        return false;
    }


    UBoxComponent* AttackZone =
        TargetHub->FindComponentByTag<UBoxComponent>(
            FName(TEXT("EnemyAttackZone"))
        );

    if (!IsValid(AttackZone))
    {
        return false;
    }

    const FVector EnemyLocation = GetActorLocation();

    const FTransform BoxTransform =
        AttackZone->GetComponentTransform();

    const FVector LocalEnemy =
        BoxTransform.InverseTransformPosition(EnemyLocation);

    const FVector BoxExtent =
        AttackZone->GetUnscaledBoxExtent();

    // Prüfen, ob der Enemy bereits innerhalb
    // des horizontalen Grundrisses der Box ist.
    const bool bInsideXY =
        FMath::Abs(LocalEnemy.X) <= BoxExtent.X &&
        FMath::Abs(LocalEnemy.Y) <= BoxExtent.Y;

    if (bInsideXY)
    {
        OutTargetLocation = EnemyLocation;
        OutDistance = 0.0f;
        return true;
    }

    // Nächsten Punkt am Box-Grundriss bestimmen.
    const FVector LocalTarget(
        FMath::Clamp(
            LocalEnemy.X, -BoxExtent.X, BoxExtent.X),
        FMath::Clamp(
            LocalEnemy.Y, -BoxExtent.Y, BoxExtent.Y),
        LocalEnemy.Z
    );

    OutTargetLocation =
        BoxTransform.TransformPosition(LocalTarget);

    FVector Direction =
        OutTargetLocation - EnemyLocation;

    Direction.Z = 0.0f;

    OutDistance = Direction.Size();

    return true;
}


void AEnemyBase::RefreshNavigationPath()
{
    UE_LOG(LogTemp, Warning,
    TEXT("NAVIGATION UPDATE: %s | State: %s"),
    *GetName(),
    *UEnum::GetValueAsString(CurrentState)
);
    
    if (CurrentState != EEnemyState::Moving ||
        !IsValid(TargetHub))
    {
        return;
    }

    UNavigationSystemV1* NavSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );

    if (!NavSystem)
    {
        UE_LOG(LogTemp, Error,
            TEXT("NAV ERROR: NavigationSystem not found!"));
        return;
    }

    FVector TargetLocation;
    float Distance;

    if (!GetHubTargetData(TargetLocation, Distance))
    {
        return;
    }

    if (Distance <= AttackRange)
    {
        return;
    }

    // Abstand von der AttackZone einplanen.
    const float StandOff =
        FMath::Max(20.0f, AttackRange * 0.6f);

    const FVector DirectionToZone =
        (TargetLocation - GetActorLocation())
        .GetSafeNormal2D();

    const FVector DesiredGoal =
        TargetLocation - DirectionToZone * StandOff;

    // Erfolgreichen Pfad nur erneuern,
    // wenn das Ziel ausreichend gewandert ist.
    if (bHasValidPath &&
        CurrentPathIndex < NavigationPoints.Num() &&
        FVector::DistSquared2D(LastRequestedGoal, DesiredGoal)
            < FMath::Square(120.0f))
    {
        return;
    }

    // Start auf das NavMesh projizieren.
    FNavLocation StartOnNav;

    if (!NavSystem->ProjectPointToNavigation(
      GetActorLocation(),
      StartOnNav,
      FVector(150.0f, 150.0f, 300.0f)
  ))
    {
        UE_LOG(LogTemp, Error,
            TEXT("NAV ERROR: Enemy start not on NavMesh!"));

        bHasValidPath = false;
        NavigationPoints.Reset();
        return;
    }

    // Hilfsfunktion für einen möglichen Zielpunkt.
    auto TryBuildPath = [&](const FVector& Candidate) -> bool
    {
        FNavLocation EndOnNav;

        if (!NavSystem->ProjectPointToNavigation(
            Candidate,
            EndOnNav,
            FVector(150.0f, 150.0f, 300.0f)
        ))
        
        {
            return false;
        }

        if (FVector::DistSquared2D(
                Candidate,
                EndOnNav.Location
            ) > FMath::Square(150.0f))
        {
            return false;
        }

        UNavigationPath* Path =
            UNavigationSystemV1::
            FindPathToLocationSynchronously(
                this,
                StartOnNav.Location,
                EndOnNav.Location,
                nullptr,
                nullptr
            );

        if (!IsValid(Path) ||
            !Path->IsValid() ||
            Path->IsPartial() ||
            Path->PathPoints.Num() < 2)
        {
            return false;
        }

        NavigationPoints = Path->PathPoints;
        CurrentPathIndex = 1;
        LastPathGoal = Candidate;
        LastRequestedGoal = DesiredGoal;
        bHasValidPath = true;

        if (bDrawNavigationDebug)
        {
            for (int32 i = 1;
                 i < NavigationPoints.Num();
                 ++i)
            {
                DrawDebugLine(
                    GetWorld(),
                    NavigationPoints[i - 1],
                    NavigationPoints[i],
                    FColor::Green,
                    false,
                    RepathInterval + 0.3f,
                    0,
                    3.0f
                );
            }
        }

        return true;
    };

    // Zuerst den nächstgelegenen Angriffspunkt.
    if (TryBuildPath(DesiredGoal))
    {
        return;
    }

    // Fallback: andere Seiten der Box ausprobieren.
    UBoxComponent* AttackZone =
        TargetHub->FindComponentByTag<UBoxComponent>(
            FName(TEXT("EnemyAttackZone"))
        );

    if (IsValid(AttackZone))
    {
        const FVector E =
            AttackZone->GetUnscaledBoxExtent();

        const FTransform T =
            AttackZone->GetComponentTransform();

        const FVector LocalPoints[] =
        {
            FVector( E.X,  0.0f, 0.0f),
            FVector(-E.X,  0.0f, 0.0f),
            FVector(0.0f,  E.Y, 0.0f),
            FVector(0.0f, -E.Y, 0.0f),
            FVector( E.X,  E.Y, 0.0f),
            FVector( E.X, -E.Y, 0.0f),
            FVector(-E.X,  E.Y, 0.0f),
            FVector(-E.X, -E.Y, 0.0f)
        };

        for (const FVector& LocalPoint : LocalPoints)
        {
            const FVector LocalDirection =
                LocalPoint.GetSafeNormal2D();

            const FVector WorldDirection =
                T.TransformVectorNoScale(LocalDirection)
                .GetSafeNormal2D();

            const FVector Candidate =
                T.TransformPosition(LocalPoint) +
                WorldDirection * StandOff;

            if (TryBuildPath(Candidate))
            {
                return;
            }
        }
    }

    // Kein vollständig erreichbarer Weg gefunden.
    NavigationPoints.Reset();
    CurrentPathIndex = 0;
    bHasValidPath = false;

    UE_LOG(LogTemp, Verbose,
        TEXT("Enemy %s: No complete navigation path"),
        *GetName()
    );
}


void AEnemyBase::MoveAlongNavigationPath(float DeltaTime)
{
    if (!bHasValidPath ||
        CurrentPathIndex >= NavigationPoints.Num())
    {
        return;
    }

    FVector CurrentLocation = GetActorLocation();

    // Bereits erreichte Wegpunkte überspringen.
    while (CurrentPathIndex < NavigationPoints.Num())
    {
        const float Distance =
            FVector::Dist2D(
                CurrentLocation,
                NavigationPoints[CurrentPathIndex]
            );

        if (Distance > WaypointAcceptanceRadius)
        {
            break;
        }

        ++CurrentPathIndex;
    }

    if (CurrentPathIndex >= NavigationPoints.Num())
    {
        bHasValidPath = false;
        return;
    }

    const FVector NextPoint =
        NavigationPoints[CurrentPathIndex];

    const FVector DesiredDirection =
        (NextPoint - CurrentLocation).GetSafeNormal2D();
    
    
    FVector MoveDirection =
        DesiredDirection +
        CachedSeparation * SeparationWeight;

    MoveDirection.Z = 0.0f;
    MoveDirection.Normalize();

    // Navigation hat Vorrang.
    // Maximal etwa 25 Grad Abweichung
    // von der geplanten Pfadrichtung.
    if (FVector::DotProduct(
            MoveDirection,
            DesiredDirection) < 0.9f)
    {
        MoveDirection = DesiredDirection;
    }
    


    const float DistanceToPoint =
        FVector::Dist2D(CurrentLocation, NextPoint);

    const float MoveDistance = FMath::Min(
        MoveSpeed * DeltaTime,
        DistanceToPoint
    );


    const FVector MoveDelta =
        MoveDirection * MoveDistance;

    FHitResult Hit;

    // Normale Bewegung entlang des NavMesh-Pfades.
    AddActorWorldOffset(
        MoveDelta,
        true,
        &Hit
    );

    // Bei einer blockierenden Kollision an der
    // Oberfläche entlanggleiten.
    if (Hit.IsValidBlockingHit() && !Hit.bStartPenetrating)
    {
        const FVector RemainingDelta =
            MoveDelta * (1.0f - Hit.Time);

        FVector SlideDelta =
            FVector::VectorPlaneProject(
                RemainingDelta,
                Hit.Normal
            );

        // Unsere Bewegung bleibt horizontal.
        SlideDelta.Z = 0.0f;

        if (!SlideDelta.IsNearlyZero())
        {
            FHitResult SlideHit;

            AddActorWorldOffset(
                SlideDelta,
                true,
                &SlideHit
            );
        }
    }
    
    
    // Prüfen, ob wir uns tatsächlich bewegt haben.
    const float ActualMovement =
        FVector::Dist2D(
            CurrentLocation,
            GetActorLocation()
        );

    if (ActualMovement < 1.0f)
    {
        StuckTime += DeltaTime;
    }
    else
    {
        StuckTime = 0.0f;
    }

    // Nach 1 Sekunde ohne Fortschritt
    // einen neuen Pfad anfordern.
    if (StuckTime >= 1.0f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Enemy %s is stuck. Requesting new path."),
            *GetName()
        );

        bHasValidPath = false;
        StuckTime = 0.0f;
    }
}

void AEnemyBase::UpdateSeparation()
{
    CachedSeparation = FVector::ZeroVector;

    if (!GetWorld() ||
        (CurrentState != EEnemyState::Moving &&
         CurrentState != EEnemyState::Attacking) ||
        SeparationRadius <= 0.0f)
    {
        return;
    }


    TArray<FOverlapResult> Overlaps;

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    const FVector MyLocation = GetActorLocation();

    GetWorld()->OverlapMultiByObjectType(
        Overlaps,
        MyLocation,
        FQuat::Identity,
        ObjectParams,
        FCollisionShape::MakeSphere(SeparationRadius),
        QueryParams
    );

    for (const FOverlapResult& Result : Overlaps)
    {
        AEnemyBase* OtherEnemy =
            Cast<AEnemyBase>(Result.GetActor());

        if (!IsValid(OtherEnemy) ||
            OtherEnemy == this ||
            OtherEnemy->CurrentState == EEnemyState::Dead)
        {
            continue;
        }

        FVector Away =
            MyLocation - OtherEnemy->GetActorLocation();

        Away.Z = 0.0f;

        const float Distance = Away.Size();

        if (Distance < KINDA_SMALL_NUMBER ||
            Distance >= SeparationRadius)
        {
            continue;
        }

        // Naher Gegner = stärkere Abstoßung
        const float Strength =
            1.0f - Distance / SeparationRadius;

        CachedSeparation +=
            Away.GetSafeNormal() * Strength;
    }

    // Betrag maximal 1, aber nicht zwangsweise
    // normalisieren. So bleibt die Stärke erhalten.
    CachedSeparation =
        CachedSeparation.GetClampedToMaxSize(1.0f);
    
    
    if (bDrawNavigationDebug &&
        !CachedSeparation.IsNearlyZero())
    {
        const FVector Start =
            GetActorLocation() + FVector(0, 0, 80);

        const FVector End =
            Start + CachedSeparation * 120.0f;

        DrawDebugDirectionalArrow(
            GetWorld(),
            Start,
            End,
            20.0f,
            FColor::Cyan,
            false,
            0.17f,
            0,
            2.0f
        );
    }
}

void AEnemyBase::MoveDuringAttack(float DeltaTime)
{
    if (!IsValid(TargetHub) ||
        CachedSeparation.IsNearlyZero() ||
        AttackSeparationSpeed <= 0.0f)
    {
        return;
    }

    FVector TargetLocation;
    float Distance = 0.0f;

    if (!GetHubTargetData(TargetLocation, Distance) ||
        Distance > AttackRange)
    {
        return;
    }

    const FVector CurrentLocation = GetActorLocation();

    // Richtung von der Angriffszone weg.
    FVector Outward =
        (CurrentLocation - TargetLocation).GetSafeNormal2D();

    // Fallback, falls wir innerhalb der Zone stehen.
    if (Outward.IsNearlyZero())
    {
        Outward =
            (CurrentLocation - TargetHub->GetActorLocation())
            .GetSafeNormal2D();
    }

    if (Outward.IsNearlyZero())
    {
        return;
    }

    // Tangente: seitlich entlang der Angriffszone.
    const FVector Tangent(
        -Outward.Y,
         Outward.X,
         0.0f
    );

    float SideInfluence =
        FVector::DotProduct(CachedSeparation, Tangent);

    // Wenn die Abstoßung fast nur radial wirkt,
    // eine kleine seitliche Ausweichrichtung wählen.
    if (FMath::Abs(SideInfluence) < 0.05f &&
        CachedSeparation.SizeSquared2D() > 0.01f)
    {
        const float SideSign =
            (GetUniqueID() % 2 == 0) ? 1.0f : -1.0f;

        SideInfluence = SideSign * 0.2f;
    }

    const FVector MoveDelta =
        Tangent *
        SideInfluence *
        AttackSeparationSpeed *
        DeltaTime;

    if (MoveDelta.IsNearlyZero())
    {
        return;
    }

    // Vorher prüfen, ob die geplante Bewegung
    // innerhalb der Angriffsreichweite bleibt.
    UBoxComponent* AttackZone =
        TargetHub->FindComponentByTag<UBoxComponent>(
            FName(TEXT("EnemyAttackZone"))
        );

    if (!IsValid(AttackZone))
    {
        return;
    }

    const FVector CandidateLocation =
        CurrentLocation + MoveDelta;

    const FTransform BoxTransform =
        AttackZone->GetComponentTransform();

    const FVector LocalCandidate =
        BoxTransform.InverseTransformPosition(CandidateLocation);

    const FVector E =
        AttackZone->GetUnscaledBoxExtent();

    const FVector LocalClosest(
        FMath::Clamp(LocalCandidate.X, -E.X, E.X),
        FMath::Clamp(LocalCandidate.Y, -E.Y, E.Y),
        LocalCandidate.Z
    );

    const FVector ClosestWorld =
        BoxTransform.TransformPosition(LocalClosest);

    FVector SurfaceOffset =
        CandidateLocation - ClosestWorld;

    SurfaceOffset.Z = 0.0f;

    if (SurfaceOffset.SizeSquared() >
        FMath::Square(AttackRange))
    {
        return;
    }

    // Bewegung mit Collision-Sweep.
    FHitResult Hit;

    AddActorWorldOffset(
        MoveDelta,
        true,
        &Hit
    );
}

