
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "HubWeaponComponent.generated.h"

class USceneComponent;
class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHubWeaponFiredSignature, USceneComponent*, FiredMuzzle);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SEMESTERPROJEKT_API UHubWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHubWeaponComponent();
	
	
	UPROPERTY(BlueprintAssignable, Category="HUB|Weapon|Effects")
	FHubWeaponFiredSignature OnWeaponFired;

	UFUNCTION(BlueprintCallable, Category="HUB|Weapon")
	void StartFire();

	UFUNCTION(BlueprintCallable, Category="HUB|Weapon")
	void StopFire();

	UFUNCTION(BlueprintCallable, Category="HUB|Weapon")
	void SetMuzzlePoints(
		USceneComponent* InLeftMuzzle,
		USceneComponent* InRightMuzzle
	);

	UFUNCTION(BlueprintCallable, Category="HUB|Weapon")
	void SetAimTarget(FVector NewAimTarget);
	

protected:
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason
	) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category="HUB|Weapon|Config",
		meta=(ClampMin="0.01"))
	float FireInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category="HUB|Weapon|Config")
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(BlueprintReadOnly, Category="HUB|Weapon")
	bool bIsFiring = false;



private:
	void Fire();

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> LeftMuzzle;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> RightMuzzle;

	FTimerHandle FireTimerHandle;

	FVector AimTarget = FVector::ZeroVector;

	bool bHasAimTarget = false;
	bool bFireLeftNext = true;
};
