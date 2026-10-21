// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyActorAA2.generated.h"

// UENUM 必须定义在类外面（全局作用域），UHT 才认
UENUM()
enum class EPetState : uint8
{
	Idle,      // 待机
	Walking,   // 走动
	Happy      // 被点击后（高兴）
};

UCLASS()
class MYPROJECTS_API AMyActorAA2 : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMyActorAA2();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "测试")
	float RotateSpeed = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pet")
	float MoveSpeed = 120.0f;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// 被鼠标点击时调用
	virtual void NotifyActorOnClicked(FKey ButtonPressed = EKeys::LeftMouseButton) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* Mesh;

	// 状态机
	EPetState CurrentState = EPetState::Idle;
	float StateTimer = 0.0f;
	float MoveDirection = 1.0f;
};
