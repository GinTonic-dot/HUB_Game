#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HubGameInstance.h"
#include "Data/HubBuildingData.h"
#include "IntermissionManager.generated.h"

UCLASS(Blueprintable)
class SEMESTERPROJEKT_API AIntermissionManager : public AActor
{
	GENERATED_BODY()

public:

	AIntermissionManager();


protected:

	virtual void BeginPlay() override;


public:

	// ---------- References ----------

	UPROPERTY(BlueprintReadOnly, Category = "Intermission")
	TObjectPtr<UHubGameInstance> GameInstanceRef;


	// ---------- Building Data ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intermission|Buildings")
	TArray<FHubBuildingData> BuildingData;


	// ---------- Building Functions ----------

	UFUNCTION(BlueprintPure, Category = "Intermission|Buildings")
	bool GetBuildingData(
		EHubBuildingType BuildingType,
		FHubBuildingData& OutBuildingData
	) const;


	UFUNCTION(BlueprintCallable, Category = "Intermission|Buildings")
	bool TryPurchaseBuilding(
		EHubBuildingType BuildingType
	);


	// ---------- Blueprint Events ----------
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Intermission")
	void OnIntermissionReady();

	UFUNCTION(BlueprintImplementableEvent, Category = "Intermission|Buildings")
	void OnBuildingPurchased(
		EHubBuildingType BuildingType,
		int32 NewLevel
	);
};