
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnHealthChangedSignature,
	float, CurrentHealth,
	float, MaxHealth
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathSignature);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))

class SEMESTERPROJEKT_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

	

public:
	UHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health",
		meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
	float CurrentHealth = 100.0f;

	UPROPERTY(BlueprintAssignable, Category="Health|Events")
	FOnHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="Health|Events")
	FOnDeathSignature OnDeath;

	UFUNCTION(BlueprintCallable, Category="Health")
	void ApplyHealthDamage(float DamageAmount);

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDead() const;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTakeAnyDamage(
		AActor* DamagedActor,
		float Damage,
		const UDamageType* DamageType,
		AController* InstigatedBy,
		AActor* DamageCauser
	);
};


