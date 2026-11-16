
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnemyBase.generated.h"

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	Moving     UMETA(DisplayName = "Moving"),
	Attacking  UMETA(DisplayName = "Attacking"),
	Stunned    UMETA(DisplayName = "Stunned"),
	Dead       UMETA(DisplayName = "Dead")
};

UCLASS()
class AEnemyBase : public AActor
{
	GENERATED_BODY()

public:
	AEnemyBase();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement")
	float MoveSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat",
		meta = (ClampMin = "0.0"))
	float AttackRange = 200.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Enemy|Target")
	TObjectPtr<AActor> TargetHub = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|State")
	EEnemyState CurrentState = EEnemyState::Moving;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "Enemy|Combat",
		meta = (ClampMin = "0.0"))
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "Enemy|Combat",
		meta = (ClampMin = "0.1"))
	float AttackCooldown = 1.5f;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "Enemy|Navigation",
		meta = (ClampMin = "0.2"))
	float RepathInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "Enemy|Navigation",
		meta = (ClampMin = "10.0"))
	float WaypointAcceptanceRadius = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "Enemy|Navigation")
	bool bDrawNavigationDebug = true;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "Enemy|Separation",
		meta = (ClampMin = "0.0"))
	float SeparationRadius = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "Enemy|Separation",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SeparationWeight = 0.35f;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "Enemy|Separation",
		meta = (ClampMin = "0.0"))
	float AttackSeparationSpeed = 75.0f;





protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Components")
	TObjectPtr<UCapsuleComponent> CollisionCapsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Components")
	TObjectPtr<UStaticMeshComponent> EnemyMesh;

	void UpdateState();
	void SetEnemyState(EEnemyState NewState);
	void HandleAttack();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
	Category = "Enemy|Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	UFUNCTION()
	void HandleDeath();
	
	
	FTimerHandle AttackTimerHandle;

	void StartAttacking();
	void StopAttacking();
	
	bool GetHubTargetData(FVector& OutTargetLocation, float& OutDistance) const;
	
	
	// Gespeicherter Weg zum HUB
	TArray<FVector> NavigationPoints;

	// Aktueller Wegpunkt
	int32 CurrentPathIndex = 0;

	// Timer zur Neuberechnung
	FTimerHandle RepathTimerHandle;

	// Zuletzt berechnete Zielposition
	FVector LastPathGoal = FVector::ZeroVector;

	// Letzter Pfad war erfolgreich
	bool bHasValidPath = false;

	void RefreshNavigationPath();
	void MoveAlongNavigationPath(float DeltaTime);
	
	FVector LastRequestedGoal = FVector::ZeroVector;
	
	float StuckTime = 0.0f;
	
	
	FVector CachedSeparation = FVector::ZeroVector;

	float SeparationUpdateElapsed = 0.0f;

	void UpdateSeparation();
	
	
	void MoveDuringAttack(float DeltaTime);

    
	// Wird beim Tod des Gegners ausgelöst.
	// Ermöglicht Death-Animationen, Niagara-Effekte
	// und Sounds direkt im jeweiligen Enemy-Blueprint.
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Death")
	void OnEnemyDeathVisuals();



};
