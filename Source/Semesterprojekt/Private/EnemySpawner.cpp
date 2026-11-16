
#include "EnemySpawner.h"

#include "EnemyBase.h"

#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "CollisionQueryParams.h"


// ============================================================
// 1. CONSTRUCTOR
// ============================================================
//
// Der EnemySpawner besitzt keine sichtbare Darstellung und
// benötigt keinen Tick.
//
// Stattdessen verwenden wir zwei Timer:
//
// SpawnTimer:
//   Erzeugt regelmäßig neue Gegnergruppen.
//
// DespawnTimer:
//   Überprüft, ob bereits gespawnte Gegner zu weit
//   vom HUB entfernt sind und entfernt werden können.
//
// Diese Trennung verhindert unnötige Prüfungen pro Frame.
// ============================================================

AEnemySpawner::AEnemySpawner()
{
    PrimaryActorTick.bCanEverTick = false;
}


// ============================================================
// 2. BEGIN PLAY
// ============================================================
//
// Beim Spielstart prüfen wir zunächst die wichtigsten
// Einstellungen und starten anschließend unsere Timer.
//
// Der HUB muss im Editor als TargetHub zugewiesen sein.
//
// SpawnRules werden ebenfalls im Blueprint konfiguriert.
// Hier legen wir später Normal, Runner und Tank fest.
// ============================================================

void AEnemySpawner::BeginPlay()
{
    Super::BeginPlay();

    if (!IsValid(TargetHub))
    {
        UE_LOG(LogTemp, Error,
            TEXT("EnemySpawner: TargetHub is missing!"));

        return;
    }

    if (SpawnRules.IsEmpty())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("EnemySpawner: No SpawnRules configured!"));

        return;
    }

    if (MaxSpawnDistance <= MinSpawnDistance)
    {
        UE_LOG(LogTemp, Error,
            TEXT("EnemySpawner: MaxSpawnDistance must be greater than MinSpawnDistance!"));

        return;
    }

    // Regelmäßige Spawn-Versuche starten.
    const float SafeSpawnInterval =
        FMath::Max(0.1f, SpawnInterval);

    GetWorldTimerManager().SetTimer(
        SpawnTimerHandle,
        this,
        &AEnemySpawner::TrySpawnGroup,
        SafeSpawnInterval,
        true
    );

    // Despawn-Prüfung unabhängig vom Spawning.
    if (bEnableDespawn)
    {
        const float SafeDespawnInterval =
            FMath::Max(0.1f, DespawnCheckInterval);

        GetWorldTimerManager().SetTimer(
            DespawnTimerHandle,
            this,
            &AEnemySpawner::DespawnDistantEnemies,
            SafeDespawnInterval,
            true
        );
    }

    UE_LOG(LogTemp, Log,
        TEXT("EnemySpawner: Initialized successfully."));
}


// ============================================================
// 3. END PLAY
// ============================================================
//
// Wird der Spawner entfernt oder das Level beendet,
// räumen wir unsere Timer auf.
//
// Die bereits gespawnten Gegner werden hier nicht zerstört.
// Sie sind eigenständige Actors und können weiterhin bestehen.
// ============================================================

void AEnemySpawner::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
    GetWorldTimerManager().ClearTimer(DespawnTimerHandle);

    TrackedEnemies.Reset();

    Super::EndPlay(EndPlayReason);
}


// ============================================================
// 4. SPAWN-VERSUCH
// ============================================================
//
// Diese Funktion wird regelmäßig vom SpawnTimer aufgerufen.
//
// Ablauf:
//
// 1. Ungültige Gegnerreferenzen entfernen.
// 2. Prüfen, ob das globale Gegnerlimit erreicht wurde.
// 3. Prüfen, ob noch neue Actors erzeugt werden dürfen.
// 4. Eine geeignete SpawnRule auswählen.
// 5. Eine neue Gegnergruppe erzeugen.
//
// Falls ein Limit erreicht wurde, passiert einfach nichts.
// Beim nächsten Timer-Durchlauf wird erneut geprüft.
// ============================================================

void AEnemySpawner::TrySpawnGroup()
{
    if (!IsValid(TargetHub))
    {
        return;
    }

    CleanupTrackedEnemies();

    // Limit für gleichzeitig lebende Gegner.
    if (GetAliveEnemyCount() >= MaxAliveEnemies)
    {
        return;
    }

    // Separates Limit für alle noch existierenden Actors.
    // Dazu zählen auch tote Gegner vor ihrer Zerstörung.
    if (GetExistingEnemyCount() >= MaxExistingEnemies)
    {
        return;
    }

    const FEnemySpawnRule* SelectedRule =
        SelectSpawnRule();

    if (!SelectedRule)
    {
        return;
    }

    SpawnGroup(*SelectedRule);
}


// ============================================================
// 5. SPAWN-RULE AUSWÄHLEN
// ============================================================
//
// Jede SpawnRule besitzt ein Gewicht.
//
// Beispiel:
//
// Normal = 70
// Runner = 25
// Tank   = 5
//
// Das entspricht einer gewichteten Zufallsauswahl.
//
// Wichtig:
//
// Die Gewichte beziehen sich auf Spawn-Ereignisse,
// NICHT auf die Anzahl einzelner Gegner.
//
// Eine Runner-Gruppe mit fünf Gegnern zählt bei der
// Auswahl genauso als ein Ereignis wie ein einzelner Tank.
//
// Regeln, deren Limits bereits erreicht sind,
// werden vor der Zufallsauswahl ausgeschlossen.
// ============================================================

const FEnemySpawnRule* AEnemySpawner::SelectSpawnRule() const
{
    const int32 FreeAliveSlots =
        FMath::Max(
            0,
            MaxAliveEnemies - GetAliveEnemyCount()
        );

    const int32 FreeActorSlots =
        FMath::Max(
            0,
            MaxExistingEnemies - GetExistingEnemyCount()
        );

    float TotalWeight = 0.0f;

    // Erstes Durchlaufen:
    // Gesamtgewicht aller erlaubten Regeln berechnen.
    for (const FEnemySpawnRule& Rule : SpawnRules)
    {
        if (!Rule.EnemyClass.Get() ||
            Rule.SpawnWeight <= 0.0f)
        {
            continue;
        }

        const int32 FreeTypeSlots =
            Rule.MaxAliveOfType -
            GetAliveCountOfType(Rule.EnemyType);

        const int32 AvailableSlots =
            FMath::Min(
                FMath::Min(
                    FreeAliveSlots,
                    FreeActorSlots
                ),
                FreeTypeSlots
            );

        const int32 MinimumGroupSize =
            FMath::Max(1, Rule.MinGroupSize);

        if (AvailableSlots < MinimumGroupSize)
        {
            continue;
        }

        TotalWeight += Rule.SpawnWeight;
    }

    if (TotalWeight <= 0.0f)
    {
        return nullptr;
    }

    // Zufallswert innerhalb des Gesamtgewichts.
    float RandomWeight =
        FMath::FRandRange(0.0f, TotalWeight);

    const FEnemySpawnRule* LastEligibleRule = nullptr;

    // Zweites Durchlaufen:
    // Gewichtete Auswahl durchführen.
    for (const FEnemySpawnRule& Rule : SpawnRules)
    {
        if (!Rule.EnemyClass.Get() ||
            Rule.SpawnWeight <= 0.0f)
        {
            continue;
        }

        const int32 FreeTypeSlots =
            Rule.MaxAliveOfType -
            GetAliveCountOfType(Rule.EnemyType);

        const int32 AvailableSlots =
            FMath::Min(
                FMath::Min(
                    FreeAliveSlots,
                    FreeActorSlots
                ),
                FreeTypeSlots
            );

        if (AvailableSlots <
            FMath::Max(1, Rule.MinGroupSize))
        {
            continue;
        }

        LastEligibleRule = &Rule;

        RandomWeight -= Rule.SpawnWeight;

        if (RandomWeight <= 0.0f)
        {
            return &Rule;
        }
    }

    return LastEligibleRule;
}


// ============================================================
// 6. GEGNERGRUPPE ERZEUGEN
// ============================================================
//
// Eine SpawnRule bestimmt:
//
// - Welche Enemy-Klasse verwendet wird.
// - Wie viele Gegner zusammen erscheinen.
// - Wie weit sie innerhalb der Gruppe verteilt sind.
// - Wie viele Gegner dieses Typs gleichzeitig leben dürfen.
//
// Wir suchen zuerst einen Gruppenschwerpunkt innerhalb
// des Spawnrings um den HUB.
//
// Anschließend suchen wir für jedes Gruppenmitglied
// eine eigene Position in der Nähe dieses Punktes.
//
// Durch unterschiedliche GroupSpreadRadius-Werte entstehen:
//
// Runner:
//   Kleine, eng beieinanderliegende Gruppen.
//
// Normal:
//   Locker verteilte Gruppen.
//
// Tank:
//   Einzelne Gegner mit geringer Spawnwahrscheinlichkeit.
// ============================================================

void AEnemySpawner::SpawnGroup(
    const FEnemySpawnRule& Rule)
{
    if (!IsValid(TargetHub) ||
        !Rule.EnemyClass.Get())
    {
        return;
    }

    UNavigationSystemV1* NavSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );

    if (!NavSystem)
    {
        return;
    }

    // HUB-Position auf das NavMesh projizieren.
    //
    // Der HUB ist groß und sein Actor-Ursprung liegt
    // möglicherweise höher als die Navigationsfläche.
    FNavLocation HubNavLocation;

    if (!NavSystem->ProjectPointToNavigation(
        TargetHub->GetActorLocation(),
        HubNavLocation,
        FVector(500.0f, 500.0f, 1000.0f)
    ))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("EnemySpawner: HUB is not near NavMesh."));

        return;
    }
    
    // Kameraabhängige Spawnentfernung berechnen.
    float EffectiveMinDistance = MinSpawnDistance;
    float EffectiveMaxDistance = MaxSpawnDistance;

    if (!CalculateAdaptiveSpawnRange(
        HubNavLocation.Location,
        EffectiveMinDistance,
        EffectiveMaxDistance
    ))
    {
        return;
    }


    // Einen erreichbaren Gruppenschwerpunkt suchen.
    FVector GroupCenter = FVector::ZeroVector;
    bool bFoundGroupCenter = false;

    const int32 Attempts =
        FMath::Max(1, MaxSpawnAttempts);

    for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
    {
        FNavLocation RandomNavLocation;

        // Die Position ist vom HUB-Navigationspunkt
        // aus über das NavMesh erreichbar.
        if (!NavSystem->GetRandomReachablePointInRadius(
            HubNavLocation.Location,
            EffectiveMaxDistance,
            RandomNavLocation
        ))
        {
            continue;
        }

        const float DistanceToHub =
            FVector::Dist2D(
                RandomNavLocation.Location,
                TargetHub->GetActorLocation()
            );

        // Der Punkt muss innerhalb unseres Rings liegen.
        if (DistanceToHub < EffectiveMinDistance ||
            DistanceToHub > EffectiveMaxDistance)
        {
            continue;
        }

        // Keine Gruppe direkt auf dem Bildschirm beginnen.
        if (bSpawnOutsideCamera &&
            IsLocationVisible(RandomNavLocation.Location))
        {
            continue;
        }

        GroupCenter = RandomNavLocation.Location;
        bFoundGroupCenter = true;
        break;
    }

    if (!bFoundGroupCenter)
    {
        return;
    }

    // Verfügbare Plätze für diese Gruppe berechnen.
    const int32 FreeAliveSlots =
        MaxAliveEnemies - GetAliveEnemyCount();

    const int32 FreeActorSlots =
        MaxExistingEnemies - GetExistingEnemyCount();

    const int32 FreeTypeSlots =
        Rule.MaxAliveOfType -
        GetAliveCountOfType(Rule.EnemyType);

    const int32 AvailableSlots =
        FMath::Max(
            0,
            FMath::Min(
                FMath::Min(
                    FreeAliveSlots,
                    FreeActorSlots
                ),
                FreeTypeSlots
            )
        );

    const int32 MinSize =
        FMath::Max(1, Rule.MinGroupSize);

    const int32 MaxSize =
        FMath::Max(MinSize, Rule.MaxGroupSize);

    const int32 DesiredGroupSize =
        FMath::RandRange(MinSize, MaxSize);

    const int32 GroupSize =
        FMath::Min(DesiredGroupSize, AvailableSlots);

    if (GroupSize < MinSize)
    {
        return;
    }

    int32 SpawnedCount = 0;

    for (int32 Index = 0; Index < GroupSize; ++Index)
    {
        // Mehrere Positionen ausprobieren.
        // Das ist beispielsweise bei engen Gängen nötig.
        for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
        {
            FVector SpawnLocation;


            if (!FindValidSpawnLocation(
                GroupCenter,
                Rule.GroupSpreadRadius,
                EffectiveMinDistance,
                EffectiveMaxDistance,
                SpawnLocation
            ))
            {
                continue;
            }


            if (!IsSpawnLocationClear(
                SpawnLocation,
                Rule.EnemyClass
            ))
            {
                continue;
            }

            if (SpawnSingleEnemy(Rule, SpawnLocation))
            {
                ++SpawnedCount;
                break;
            }
        }
    }

    if (SpawnedCount > 0)
    {
        UE_LOG(LogTemp, Log,
            TEXT("EnemySpawner: Spawned %d enemies of type %d."),
            SpawnedCount,
            static_cast<int32>(Rule.EnemyType)
        );
    }
}


// ============================================================
// 7. EINZELNEN GEGNER SPAWNEN
// ============================================================
//
// Diese Funktion erzeugt genau einen Enemy Actor.
//
// Wir verwenden Deferred Spawning, damit wir die
// TargetHub-Referenz setzen können, BEVOR BeginPlay
// am Gegner ausgeführt wird.
//
// Das ist wichtig, da unsere EnemyBase bereits
// bei ihren ersten Updates auf den HUB zugreift.
//
// Der Spawnpunkt liegt auf dem NavMesh.
// Da unsere Capsule um ihren Mittelpunkt zentriert ist,
// heben wir den Actor um seine halbe Kapselhöhe an.
// ============================================================

bool AEnemySpawner::SpawnSingleEnemy(
    const FEnemySpawnRule& Rule,
    const FVector& SpawnLocation)
{
    if (!IsValid(TargetHub) ||
        !Rule.EnemyClass.Get())
    {
        return false;
    }

    if (!IsSpawnLocationClear(
        SpawnLocation,
        Rule.EnemyClass
    ))
    {
        return false;
    }

    const AEnemyBase* EnemyDefault =
        Rule.EnemyClass.GetDefaultObject();

    if (!EnemyDefault)
    {
        return false;
    }

    const UCapsuleComponent* Capsule =
        EnemyDefault->FindComponentByClass<UCapsuleComponent>();

    const float HalfHeight =
        Capsule ? Capsule->GetScaledCapsuleHalfHeight()
                : 88.0f;

    const FVector ActualSpawnLocation =
        SpawnLocation +
        FVector(0.0f, 0.0f, HalfHeight + 2.0f);

    // Orientierung zunächst zufällig.
    // Die spätere Enemy-Bewegung übernimmt das Steering.
    const FRotator SpawnRotation(
        0.0f,
        FMath::FRandRange(0.0f, 360.0f),
        0.0f
    );

    const FTransform SpawnTransform(
        SpawnRotation,
        ActualSpawnLocation
    );

    AEnemyBase* SpawnedEnemy =
        GetWorld()->SpawnActorDeferred<AEnemyBase>(
            Rule.EnemyClass.Get(),
            SpawnTransform,
            this,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

    if (!IsValid(SpawnedEnemy))
    {
        return false;
    }

    // Ziel setzen, bevor der Actor fertig gespawnt wird.
    SpawnedEnemy->TargetHub = TargetHub;

    UGameplayStatics::FinishSpawningActor(
        SpawnedEnemy,
        SpawnTransform
    );

    // Gegner im lokalen Spawner-Register speichern.
    FTrackedEnemy Tracked;
    Tracked.Enemy = SpawnedEnemy;
    Tracked.Type = Rule.EnemyType;

    TrackedEnemies.Add(Tracked);

    return true;
}


// ============================================================
// 8. GÜLTIGE SPAWNPOSITION SUCHEN
// ============================================================
//
// Sucht einen Spawnpunkt in der Nähe eines
// bereits festgelegten Gruppenschwerpunkts.
//
// MinDistance und MaxDistance werden von SpawnGroup()
// übergeben. Dadurch funktionieren sowohl der normale
// als auch der kameraabhängige Spawnring.
//
// Prüfungen:
// 1. Ist ein HUB vorhanden?
// 2. Ist eine NavMesh-Position erreichbar?
// 3. Liegt der Punkt im erlaubten Spawnring?
// 4. Liegt der Punkt außerhalb der Kamerasicht?
//
// Die Kollisionsprüfung erfolgt anschließend separat.
// ============================================================




// ============================================================
// 8. GÜLTIGE SPAWNPOSITION SUCHEN
// ============================================================
//
// Die einzelnen Gruppenmitglieder sollen nicht alle
// auf exakt derselben Position erscheinen.
//
// Stattdessen suchen wir einen zufälligen Punkt in der
// Umgebung des Gruppenschwerpunkts.
//
// Auch dieser Punkt muss auf einer erreichbaren
// Navigationsfläche liegen.
//
// Zusätzlich prüfen wir:
//
// - Liegt der Punkt innerhalb des globalen Spawnrings?
// - Ist er außerhalb der Kamerasicht?
//
// Die eigentliche Kollisionsprüfung erfolgt anschließend
// in IsSpawnLocationClear().
// ============================================================

bool AEnemySpawner::FindValidSpawnLocation(
    const FVector& GroupCenter,
    float SpreadRadius,
    float MinDistance,
    float MaxDistance,
    FVector& OutLocation
) const
{
    if (!IsValid(TargetHub))
    {
        return false;
    }

    UNavigationSystemV1* NavSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );

    if (!NavSystem)
    {
        return false;
    }

    FVector Candidate = GroupCenter;

    // Bei Gruppen einen zufälligen erreichbaren
    // Punkt in der Nähe des Gruppenmittelpunkts suchen.
    if (SpreadRadius > KINDA_SMALL_NUMBER)
    {
        FNavLocation RandomPoint;

        if (!NavSystem->GetRandomReachablePointInRadius(
            GroupCenter,
            SpreadRadius,
            RandomPoint
        ))
        {
            return false;
        }

        Candidate = RandomPoint.Location;
    }

    // Abstand des möglichen Spawnpunkts zum HUB.
    const float DistanceToHub =
        FVector::Dist2D(
            Candidate,
            TargetHub->GetActorLocation()
        );

    // Prüfen, ob der Punkt im aktuellen Spawnring liegt.
    if (DistanceToHub < MinDistance ||
        DistanceToHub > MaxDistance)
    {
        return false;
    }

    // Keine Gegner innerhalb des Kamerabilds erzeugen.
    if (bSpawnOutsideCamera &&
        IsLocationVisible(Candidate))
    {
        return false;
    }

    OutLocation = Candidate;

    return true;
}


// ============================================================
// 9. SPAWNPOSITION AUF KOLLISIONEN PRÜFEN
// ============================================================
//
// Ein Punkt auf dem NavMesh ist nicht automatisch ein
// gültiger Spawnpunkt.
//
// Beispielsweise kann dort ein anderer Actor stehen,
// der nicht in der Navigation berücksichtigt wurde.
//
// Wir verwenden die Capsule der ausgewählten Enemy-Klasse,
// um blockierende Kollisionen zu erkennen.
//
// Zusätzlich vermeiden wir zu geringe Abstände zu
// bereits lebenden Gegnern.
//
// Dieser Mindestabstand ist bewusst einfach gehalten.
// Später kann der EnemyManager eine effizientere
// räumliche Nachbarschaftssuche übernehmen.
// ============================================================

bool AEnemySpawner::IsSpawnLocationClear(
    const FVector& Location,
    TSubclassOf<AEnemyBase> EnemyClass) const
{
    if (!GetWorld() || !EnemyClass.Get())
    {
        return false;
    }

    const AEnemyBase* EnemyDefault =
        EnemyClass.GetDefaultObject();

    if (!EnemyDefault)
    {
        return false;
    }

    const UCapsuleComponent* Capsule =
        EnemyDefault->FindComponentByClass<UCapsuleComponent>();

    const float Radius =
        Capsule ? Capsule->GetScaledCapsuleRadius()
                : 42.0f;

    const float HalfHeight =
        Capsule ? Capsule->GetScaledCapsuleHalfHeight()
                : 88.0f;

    const FVector CapsuleCenter =
        Location + FVector(
            0.0f,
            0.0f,
            HalfHeight + 2.0f
        );

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    // Prüft, ob unsere Capsule an der Position
    // eine blockierende Weltkollision hätte.
    const bool bBlocked =
        GetWorld()->OverlapBlockingTestByChannel(
            CapsuleCenter,
            FQuat::Identity,
            ECC_Pawn,
            FCollisionShape::MakeCapsule(
                Radius,
                HalfHeight
            ),
            QueryParams,
            FCollisionResponseParams::DefaultResponseParam
        );

    if (bBlocked)
    {
        return false;
    }

    // Auch bereits gespawnte Gegner berücksichtigen.
    //
    // Da unsere Enemy-Capsules einander nicht blockieren,
    // reicht die reine Physics-Abfrage hierfür nicht aus.
    const float MinimumSpacing = Radius * 2.0f + 20.0f;

    for (const FTrackedEnemy& Entry : TrackedEnemies)
    {
        const AEnemyBase* OtherEnemy =
            Entry.Enemy.Get();

        if (!IsEnemyAlive(OtherEnemy))
        {
            continue;
        }

        if (FVector::DistSquared2D(
                Location,
                OtherEnemy->GetActorLocation()
            ) < FMath::Square(MinimumSpacing))
        {
            return false;
        }
    }

    return true;
}


// ============================================================
// 10. KAMERASICHT PRÜFEN
// ============================================================
//
// Gegner sollen nach Möglichkeit außerhalb des sichtbaren
// Bildschirms erscheinen.
//
// Wir verwenden PlayerController 0, da unser Semesterprojekt
// aktuell als Einzelspielerspiel geplant ist.
//
// Ablauf:
//
// 1. Kameraposition und Rotation bestimmen.
// 2. Prüfen, ob der Punkt vor der Kamera liegt.
// 3. Weltposition in Bildschirmkoordinaten umrechnen.
// 4. Prüfen, ob sie im sichtbaren Bereich liegt.
//
// ScreenMargin erweitert den verbotenen Bildschirmbereich.
//
// Wir prüfen einen niedrigen und einen höheren Punkt.
// Dadurch wird nicht nur der Bodenpunkt berücksichtigt.
//
// Einschränkung:
// Diese Prüfung berücksichtigt den Bildschirmausschnitt,
// aber keine Verdeckung durch Wände oder Gebäude.
// ============================================================


bool AEnemySpawner::IsLocationVisible(
    const FVector& Location) const
{
    if (!bSpawnOutsideCamera)
    {
        return false;
    }

    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(this, 0);

    if (!IsValid(PlayerController))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Spawner Visibility: PlayerController missing!"));

        return true;
    }

    int32 ViewportWidth = 0;
    int32 ViewportHeight = 0;

    PlayerController->GetViewportSize(
        ViewportWidth,
        ViewportHeight
    );

    if (ViewportWidth <= 0 || ViewportHeight <= 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Spawner Visibility: Invalid viewport size!"));

        return true;
    }

    FVector CameraLocation;
    FRotator CameraRotation;

    PlayerController->GetPlayerViewPoint(
        CameraLocation,
        CameraRotation
    );

    const FVector CameraForward =
        CameraRotation.Vector();

    // Prüft einen einzelnen Punkt auf dem Bildschirm.
    auto IsPointVisible = [&](const FVector& Point) -> bool
    {
        const FVector ToPoint = Point - CameraLocation;

        // Punkte hinter der Kamera gelten als unsichtbar.
        if (FVector::DotProduct(
            CameraForward,
            ToPoint) <= 0.0f)
        {
            return false;
        }

        FVector2D ScreenPosition;

        const bool bProjected =
            PlayerController->ProjectWorldLocationToScreen(
                Point,
                ScreenPosition,
                false
            );

        // Ein nicht projizierbarer Punkt wird hier
        // als außerhalb des Bildschirms betrachtet.
        // Dies ist eine vereinfachte Annahme für
        // unsere Einzelspieler-Kamera.
        if (!bProjected)
        {
            return false;
        }

        // Sicherheitsrand um das Kamerabild.
        const float Margin = FMath::Max(0.0f, ScreenMargin);

        const bool bInsideHorizontal =
            ScreenPosition.X >= -Margin &&
            ScreenPosition.X <= ViewportWidth + Margin;

        const bool bInsideVertical =
            ScreenPosition.Y >= -Margin &&
            ScreenPosition.Y <= ViewportHeight + Margin;

        return bInsideHorizontal && bInsideVertical;
    };

    // Wir prüfen sowohl den unteren als auch
    // den oberen Bereich eines typischen Gegners.
    return
        IsPointVisible(Location + FVector(0, 0, 80)) ||
        IsPointVisible(Location + FVector(0, 0, 180));
}


// ============================================================
// 11. UNGÜLTIGE GEGNERREFERENZEN ENTFERNEN
// ============================================================
//
// Wir speichern Weak Pointers auf gespawnte Gegner.
//
// Wenn ein Gegner durch Destroy() oder SetLifeSpan()
// entfernt wird, darf diese Referenz nicht mehr
// als existierender Gegner gezählt werden.
//
// CleanupTrackedEnemies() entfernt solche Einträge.
//
// Die Funktion wird vor neuen Spawn-Entscheidungen
// und während der Despawn-Prüfung aufgerufen.
// ============================================================

void AEnemySpawner::CleanupTrackedEnemies()
{
    TrackedEnemies.RemoveAll(
        [](const FTrackedEnemy& Entry)
        {
            return !Entry.Enemy.IsValid();
        }
    );
}


// ============================================================
// 12. ENTFERNTE GEGNER DESPAWNEN
// ============================================================
//
// Der HUB bewegt sich ständig durch das Level.
//
// Gegner, die weit hinter ihm zurückbleiben,
// sollen nicht dauerhaft die Gegnerlimits belegen.
//
// Ein Gegner wird nur entfernt, wenn:
//
// - Er weiter als DespawnDistance entfernt ist.
// - Er außerhalb der Kamerasicht liegt.
//
// So vermeiden wir, dass Gegner direkt vor dem Spieler
// plötzlich verschwinden.
//
// Diese Funktion betrifft ausschließlich Gegner,
// die von diesem Spawner erzeugt wurden.
// ============================================================

void AEnemySpawner::DespawnDistantEnemies()
{
    if (!bEnableDespawn || !IsValid(TargetHub))
    {
        return;
    }

    CleanupTrackedEnemies();

    const FVector HubLocation =
        TargetHub->GetActorLocation();

    for (FTrackedEnemy& Entry : TrackedEnemies)
    {
        AEnemyBase* Enemy = Entry.Enemy.Get();

        if (!IsValid(Enemy))
        {
            continue;
        }

        const float Distance =
            FVector::Dist2D(
                Enemy->GetActorLocation(),
                HubLocation
            );

        if (Distance <= DespawnDistance)
        {
            continue;
        }

        if (IsLocationVisible(Enemy->GetActorLocation()))
        {
            continue;
        }

        Enemy->Destroy();
    }

    CleanupTrackedEnemies();
}


// ============================================================
// 13. PRÜFEN, OB EIN GEGNER LEBT
// ============================================================
//
// Unsere EnemyBase besitzt bereits eine State Machine.
//
// Ein Gegner im State Dead zählt nicht mehr zum
// lebenden Gegnerbudget.
//
// Er zählt allerdings weiterhin zum Actor-Budget,
// bis sein Actor tatsächlich zerstört wurde.
//
// Voraussetzung:
// HandleDeath() der EnemyBase muss den State Dead setzen.
// ============================================================

bool AEnemySpawner::IsEnemyAlive(
    const AEnemyBase* Enemy) const
{
    return IsValid(Enemy) &&
        Enemy->CurrentState != EEnemyState::Dead;
}


// ============================================================
// 14. ANZAHL LEBENDER GEGNER
// ============================================================
//
// Wird verwendet, um MaxAliveEnemies einzuhalten.
//
// Dabei werden ausschließlich die Gegner gezählt,
// die dieser Spawner selbst erzeugt hat.
// ============================================================

int32 AEnemySpawner::GetAliveEnemyCount() const
{
    int32 Count = 0;

    for (const FTrackedEnemy& Entry : TrackedEnemies)
    {
        if (IsEnemyAlive(Entry.Enemy.Get()))
        {
            ++Count;
        }
    }

    return Count;
}


// ============================================================
// 15. ANZAHL EXISTIERENDER ENEMY ACTORS
// ============================================================
//
// Im Gegensatz zu GetAliveEnemyCount() zählen wir hier
// auch tote Gegner, solange ihre Actors noch existieren.
//
// Das verhindert, dass Death-Animationen, Leichen und
// neue Gegner zusammen unbegrenzt viele Actors erzeugen.
// ============================================================

int32 AEnemySpawner::GetExistingEnemyCount() const
{
    int32 Count = 0;

    for (const FTrackedEnemy& Entry : TrackedEnemies)
    {
        if (Entry.Enemy.IsValid())
        {
            ++Count;
        }
    }

    return Count;
}


// ============================================================
// 16. ANZAHL LEBENDER GEGNER EINES BESTIMMTEN TYPS
// ============================================================
//
// Jeder Gegnertyp besitzt ein eigenes Limit.
//
// Beispiel:
//
// Normal: maximal 20
// Runner: maximal 12
// Tank:    maximal 2
//
// So verhindern wir, dass zufällig zu viele Tanks
// gleichzeitig auftreten.
//
// Die Einschränkung wird bei der Auswahl einer
// SpawnRule und bei der Gruppengröße berücksichtigt.
// ============================================================

int32 AEnemySpawner::GetAliveCountOfType(
    EEnemySpawnType Type) const
{
    int32 Count = 0;

    for (const FTrackedEnemy& Entry : TrackedEnemies)
    {
        if (Entry.Type != Type)
        {
            continue;
        }

        if (IsEnemyAlive(Entry.Enemy.Get()))
        {
            ++Count;
        }
    }

    return Count;
}


// ============================================================
// ADAPTIVE SPAWN DISTANCE
// ============================================================
//
// Aufgabe:
// Den Spawnring automatisch an die Kamerasicht anpassen.
//
// Vorgehen:
// 1. Die vier Bildschirmecken bestimmen.
// 2. Von jeder Ecke einen Strahl in die Welt berechnen.
// 3. Schnittpunkte mit der horizontalen NavMesh-Ebene
//    ermitteln.
// 4. Größte Entfernung dieser Punkte zum HUB bestimmen.
// 5. Einen Sicherheitsabstand addieren.
//
// Falls die Berechnung nicht möglich ist, verwenden wir
// unsere bisherigen manuell eingestellten Distanzen.
//
// Die Funktion sucht selbst KEINE Spawnpositionen.
// Sie berechnet ausschließlich die erlaubte Entfernung.
// ============================================================

bool AEnemySpawner::CalculateAdaptiveSpawnRange(
    const FVector& HubNavLocation,
    float& OutMinDistance,
    float& OutMaxDistance
) const
{
    // Standardwerte als Fallback.
    OutMinDistance = MinSpawnDistance;
    OutMaxDistance = MaxSpawnDistance;

    if (!bAdaptiveSpawnDistance ||
        !bSpawnOutsideCamera)
    {
        return true;
    }

    APlayerController* PC =
        UGameplayStatics::GetPlayerController(this, 0);

    if (!IsValid(PC))
    {
        return true;
    }

    int32 Width = 0;
    int32 Height = 0;

    PC->GetViewportSize(Width, Height);

    if (Width <= 0 || Height <= 0)
    {
        return true;
    }

    // Die vier Ecken des sichtbaren Bildschirms.
    const FVector2D Corners[] =
    {
        FVector2D(0.0, 0.0),
        FVector2D(static_cast<double>(Width), 0.0),
        FVector2D(0.0, static_cast<double>(Height)),
        FVector2D(
            static_cast<double>(Width),
            static_cast<double>(Height)
        )
    };

    double MaxVisibleDistance = 0.0;

    for (const FVector2D& Corner : Corners)
    {
        FVector RayOrigin;
        FVector RayDirection;

        if (!PC->DeprojectScreenPositionToWorld(
            Corner.X,
            Corner.Y,
            RayOrigin,
            RayDirection
        ))
        {
            return true;
        }

        // Die Blickrichtung muss nach unten zeigen,
        // um die horizontale Bodenebene zu schneiden.
        if (RayDirection.Z >= -0.001)
        {
            return true;
        }

        const double T =
            (HubNavLocation.Z - RayOrigin.Z) /
            RayDirection.Z;

        // Ungültige oder extrem entfernte Schnittpunkte
        // nicht für die Radiusberechnung verwenden.
        if (T <= 0.0 || T > 200000.0)
        {
            return true;
        }

        const FVector GroundPoint =
            RayOrigin + RayDirection * T;

        MaxVisibleDistance = FMath::Max(
            MaxVisibleDistance,
            FVector::Dist2D(
                GroundPoint,
                HubNavLocation
            )
        );
    }

    // Der neue Spawnring beginnt hinter der
    // äußersten sichtbaren Bodenposition.
    const float DesiredMinDistance =
        FMath::Max(
            MinSpawnDistance,
            static_cast<float>(MaxVisibleDistance) +
                CameraSpawnPadding
        );

    // Die ursprüngliche Breite des Spawnrings erhalten.
    const float OriginalRingWidth =
        FMath::Max(
            100.0f,
            MaxSpawnDistance - MinSpawnDistance
        );

    float AllowedMaxDistance =
        MaxAdaptiveSpawnDistance;

    // Gegner nicht direkt außerhalb der Despawn-Grenze
    // erzeugen. 250 cm bleiben als kleiner Puffer.
    if (bEnableDespawn)
    {
        AllowedMaxDistance = FMath::Min(
            AllowedMaxDistance,
            DespawnDistance - 250.0f
        );
    }


    if (DesiredMinDistance >= AllowedMaxDistance)
    {
        // Die Kamera sieht weiter als unser erlaubter
        // maximaler Spawnradius.
        //
        // Wir brechen nicht mehr direkt ab.
        // Stattdessen versuchen wir, im äußeren Bereich
        // des erlaubten Spawnrings Gegner zu platzieren.
        //
        // Die eigentliche Kameraprüfung erfolgt weiterhin
        // für jeden einzelnen Spawnpunkt.

        const float FallbackRingWidth = 1000.0f;

        OutMaxDistance = AllowedMaxDistance;

        OutMinDistance = FMath::Max(
            MinSpawnDistance,
            AllowedMaxDistance - FallbackRingWidth
        );

        UE_LOG(LogTemp, Warning,
            TEXT("Adaptive Spawn: DesiredMin=%.0f | AllowedMax=%.0f | FallbackRing=%.0f-%.0f"),
            DesiredMinDistance,
            AllowedMaxDistance,
            OutMinDistance,
            OutMaxDistance
        );

        return OutMaxDistance > OutMinDistance;
    }


    OutMinDistance = DesiredMinDistance;

    OutMaxDistance = FMath::Min(
        AllowedMaxDistance,
        DesiredMinDistance + OriginalRingWidth
    );

    return OutMaxDistance > OutMinDistance;
}
