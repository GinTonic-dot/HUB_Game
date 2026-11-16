
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class AEnemyBase;


// ============================================
// Enemy Types
// ============================================

UENUM(BlueprintType)
enum class EEnemySpawnType : uint8
{
    Normal UMETA(DisplayName = "Normal"),
    Runner UMETA(DisplayName = "Runner"),
    Tank   UMETA(DisplayName = "Tank")
};


// ============================================
// Spawn Rule
// ============================================

USTRUCT(BlueprintType)
struct FEnemySpawnRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Enemy")
    EEnemySpawnType EnemyType = EEnemySpawnType::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Enemy")
    TSubclassOf<AEnemyBase> EnemyClass;

    // Relatives Auswahlgewicht
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawn",
        meta = (ClampMin = "0.0"))
    float SpawnWeight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawn",
        meta = (ClampMin = "1"))
    int32 MinGroupSize = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawn",
        meta = (ClampMin = "1"))
    int32 MaxGroupSize = 3;

    // Abstand der Gruppenmitglieder zueinander
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawn",
        meta = (ClampMin = "0.0"))
    float GroupSpreadRadius = 250.0f;

    // Maximale lebende Gegner dieses Typs
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawn",
        meta = (ClampMin = "1"))
    int32 MaxAliveOfType = 15;
};


// ============================================
// Enemy Spawner
// ============================================

UCLASS()
class SEMESTERPROJEKT_API AEnemySpawner : public AActor
{
    GENERATED_BODY()

public:

    AEnemySpawner();

    // ========================================
    // Target
    // ========================================

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite,
        Category = "Spawner|Target")
    TObjectPtr<AActor> TargetHub = nullptr;


    // ========================================
    // Spawn Settings
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Settings",
        meta = (ClampMin = "0.1"))
    float SpawnInterval = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Settings",
        meta = (ClampMin = "1"))
    int32 MaxAliveEnemies = 25;

    // Einschließlich noch vorhandener toter Actors
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Settings",
        meta = (ClampMin = "1"))
    int32 MaxExistingEnemies = 35;


    // ========================================
    // Spawn Distance
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Distance",
        meta = (ClampMin = "0.0"))
    float MinSpawnDistance = 2500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Distance",
        meta = (ClampMin = "0.0"))
    float MaxSpawnDistance = 4000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Distance",
        meta = (ClampMin = "1"))
    int32 MaxSpawnAttempts = 20;


    // ========================================
    // Camera Visibility
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Visibility")
    bool bSpawnOutsideCamera = true;

    // Zusätzlicher Sicherheitsrand in Pixeln
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Visibility",
        meta = (ClampMin = "0.0"))
    float ScreenMargin = 150.0f;


    // ========================================
    // Despawn Settings
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Despawn")
    bool bEnableDespawn = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Despawn",
        meta = (ClampMin = "0.0"))
    float DespawnDistance = 6000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Despawn",
        meta = (ClampMin = "0.1"))
    float DespawnCheckInterval = 5.0f;


    // ========================================
    // Enemy Rules
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Enemies")
    TArray<FEnemySpawnRule> SpawnRules;
    
    
    // ========================================
    // Adaptive Spawn Distance
    // ========================================

    // Spawnring an die sichtbare Kamerafläche anpassen.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Distance|Adaptive")
    bool bAdaptiveSpawnDistance = true;

    // Zusätzlicher Abstand hinter der sichtbaren Fläche
    // in Unreal Units (cm).
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Distance|Adaptive",
        meta = (ClampMin = "0.0"))
    float CameraSpawnPadding = 500.0f;

    // Harte Obergrenze: Gegner sollen auch bei starkem
    // Zoom nicht beliebig weit entfernt spawnen.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawner|Distance|Adaptive",
        meta = (ClampMin = "100.0"))
    float MaxAdaptiveSpawnDistance = 6000.0f;



protected:

    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;



private:

    // ========================================
    // Internal Tracking
    // ========================================

    struct FTrackedEnemy
    {
        TWeakObjectPtr<AEnemyBase> Enemy;
        EEnemySpawnType Type = EEnemySpawnType::Normal;
    };

    TArray<FTrackedEnemy> TrackedEnemies;

    FTimerHandle SpawnTimerHandle;
    FTimerHandle DespawnTimerHandle;


    // ========================================
    // Spawning
    // ========================================

    void TrySpawnGroup();

    const FEnemySpawnRule* SelectSpawnRule() const;

    void SpawnGroup(const FEnemySpawnRule& Rule);

    bool SpawnSingleEnemy(
        const FEnemySpawnRule& Rule,
        const FVector& SpawnLocation
    );


    // ========================================
    // Spawn Position
    // ========================================

    bool FindValidSpawnLocation(
        const FVector& GroupCenter,
        float SpreadRadius,
        float MinDistance,
        float MaxDistance,
        FVector& OutLocation
    ) const;


    bool IsSpawnLocationClear(
        const FVector& Location,
        TSubclassOf<AEnemyBase> EnemyClass
    ) const;

    bool IsLocationVisible(
        const FVector& Location
    ) const;


    // ========================================
    // Enemy Management
    // ========================================

    void CleanupTrackedEnemies();

    void DespawnDistantEnemies();

    bool IsEnemyAlive(
        const AEnemyBase* Enemy
    ) const;

    int32 GetAliveEnemyCount() const;

    int32 GetExistingEnemyCount() const;

    int32 GetAliveCountOfType(
        EEnemySpawnType Type
    ) const;
    
    
    // Ermittelt einen passenden Spawnring anhand der
    // aktuellen Kamera und der NavMesh-Bodenhöhe.
    bool CalculateAdaptiveSpawnRange(
        const FVector& HubNavLocation,
        float& OutMinDistance,
        float& OutMaxDistance
    ) const;

};
