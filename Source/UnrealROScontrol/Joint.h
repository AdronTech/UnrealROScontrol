// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "EngineUtils.h"
#include "Joint.generated.h"


UENUM(BlueprintType)		//"BlueprintType" is essential to include
enum class EJointTypeEnum : uint8
{
	JTE_Velocity 	UMETA(DisplayName = "Velocity"),
	JTE_Position	UMETA(DisplayName = "Position"),
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UNREALROSCONTROL_API UJoint : public USceneComponent
{
	GENERATED_BODY()


	/** Pointer to first Actor to constrain.  */
	UPROPERTY(EditInstanceOnly, Category = Constraint)
		AActor* ConstraintActor1;

	/**
	 *	Name of first component property to constrain. If Actor1 is NULL, will look within Owner.
	 *	If this is NULL, will use RootComponent of Actor1
	 */
	UPROPERTY(EditAnywhere, Category = Constraint)
		FConstrainComponentPropName ComponentName1;


	/** Pointer to second Actor to constrain. */
	UPROPERTY(EditInstanceOnly, Category = Constraint)
		AActor* ConstraintActor2;

	/**
	 *	Name of second component property to constrain. If Actor2 is NULL, will look within Owner.
	 *	If this is NULL, will use RootComponent of Actor2
	 */
	UPROPERTY(EditAnywhere, Category = Constraint)
		FConstrainComponentPropName ComponentName2;


	/** Allows direct setting of first component to constraint. */
	TWeakObjectPtr<UPrimitiveComponent> OverrideComponent1;

	/** Allows direct setting of second component to constraint. */
	TWeakObjectPtr<UPrimitiveComponent> OverrideComponent2;


	UPROPERTY(instanced)
		class UPhysicsConstraintTemplate* ConstraintSetup_DEPRECATED;

	/** Notification when constraint is broken. */
	UPROPERTY(BlueprintAssignable)
		FConstraintBrokenSignature OnConstraintBroken;


public:	
	UPROPERTY(EditAnywhere, Category = Joint)
	FString Label;

	UPROPERTY(EditAnywhere, Category = Joint)
	EJointTypeEnum JointType;

	// Sets default values for this component's properties
	UJoint();

	virtual void BeginPlay() override;

	void ExecuteCommand(double command);
	float GetAngle();
	void SetAngle(float value);
	void SetAngularVelocity(float value);


	/** All constraint settings */
	UPROPERTY(EditAnywhere, Category = ConstraintComponent, meta = (ShowOnlyInnerProperties))
		FConstraintInstance			ConstraintInstance;

	//Begin UObject Interface
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//End UObject interface

	//Begin ActorComponent interface
#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual void OnRegister() override;
#endif // WITH_EDITOR
	virtual void OnUnregister() override;
	virtual void InitializeComponent() override;
	//End ActorComponent interface

	//~ Begin SceneComponent Interface
#if WITH_EDITOR
	virtual void PostEditComponentMove(bool bFinished) override;
#endif // WITH_EDITOR
	//~ End SceneComponent Interface

	/** Get the body frame. Works without constraint being created */
	FTransform GetBodyTransform(EConstraintFrame::Type Frame) const;

	/** Get body bounding box. Works without constraint being created */
	FBox GetBodyBox(EConstraintFrame::Type Frame) const;

	/** Initialize the frames and creates constraint */
	void InitComponentConstraint();

	/** Break the constraint */
	void TermComponentConstraint();

	/** Directly specify component to connect. Will update frames based on current position. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|Joint")
		void SetConstrainedComponents(UPrimitiveComponent* Component1, FName BoneName1, UPrimitiveComponent* Component2, FName BoneName2);

	/** Break this constraint */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|Joint")
		void BreakConstraint();


	/**
	 *	Update the reference frames held inside the constraint that indicate the joint location in the reference frame
	 *	of the two connected bodies. You should call this whenever the constraint or either Component moves, or if you change
	 *	the connected Components. This function does nothing though once the joint has been initialized.
	 */
	void UpdateConstraintFrames();


#if WITH_EDITOR
	void UpdateSpriteTexture();
#endif

protected:

	friend class FConstraintComponentVisualizer;

	/** Get the body instance that we want to constrain to */
	FBodyInstance* GetBodyInstance(EConstraintFrame::Type Frame) const;

	/** Internal util to get body transform from actor/component name/bone name information */
	FTransform GetBodyTransformInternal(EConstraintFrame::Type Frame, FName InBoneName) const;
	/** Internal util to get body box from actor/component name/bone name information */
	FBox GetBodyBoxInternal(EConstraintFrame::Type Frame, FName InBoneName) const;
	/** Internal util to get component from actor/component name */
	UPrimitiveComponent* GetComponentInternal(EConstraintFrame::Type Frame) const;

	/** Routes the FConstraint callback to the dynamic delegate */
	void OnConstraintBrokenHandler(FConstraintInstance* BrokenConstraint);

	/** Returns the scale of the constraint as it will be passed into the ConstraintInstance*/
	float GetConstraintScale() const;

private:
	/** Wrapper that calls our constraint broken delegate */
	void OnConstraintBrokenWrapper(int32 ConstraintIndex);
};
