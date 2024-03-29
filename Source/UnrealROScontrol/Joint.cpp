// Fill out your copyright notice in the Description page of Project Settings.


#include "Joint.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/ConstraintUtils.h"
#include "Components/BillboardComponent.h"
#include "JointManager.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "Joint"


// Sets default values for this component's properties
UJoint::UJoint()
{
	bWantsInitializeComponent = true;
#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif

	ConstraintInstance.SetAngularSwing1Motion(ACM_Locked);
	ConstraintInstance.SetAngularSwing2Motion(ACM_Locked);
	ConstraintInstance.SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
	ConstraintInstance.SetAngularVelocityDriveTwistAndSwing(true, false);
	ConstraintInstance.SetAngularDriveParams(1000, 100, 0);
	ConstraintInstance.SetDisableCollision(true);
	ConstraintInstance.ProfileInstance.ProjectionAngularTolerance = 0.01;
	ConstraintInstance.ProfileInstance.ProjectionLinearTolerance = 0.01;
	//ConstraintInstance.ProfileInstance.bParentDominates = true;
}

void UJoint::BeginPlay() 
{
	Super::BeginPlay();
	
	for (TActorIterator<AJointManager> Itr(GetWorld()); Itr; ++Itr)
	{
		// Access the subclass instance with the * or -> operators.
		AJointManager *JointManager = *Itr;
		JointManager->Subscribe(this);
	}

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(TimerHandle, this, &UJoint::CalcVelocity, 0.02f, true);
	}

	oldAngle = GetAngle();
	oldPosition = GetAngle();
}

void UJoint::ExecuteCommand(double command)
{
	switch (JointType)
	{
	case EJointTypeEnum::JTE_Position:
		SetAngle(command);
		break;
	case EJointTypeEnum::JTE_Velocity:
		SetAngularVelocity(command);
		break;
	default:
		UE_LOG(LogTemp, Error, TEXT("Something went wrong with the JointType"));
		break;
	}
}

float UJoint::GetAngle()
{
	return position;
}

void UJoint::SetAngle(float value)
{
	ConstraintInstance.SetOrientationDriveTwistAndSwing(true, false);
	ConstraintInstance.SetAngularVelocityTarget(FVector(0, 0, 0));
	FQuat pos = FQuat::MakeFromEuler(FVector(FMath::RadiansToDegrees(-value), 0, 0));
	ConstraintInstance.SetAngularOrientationTarget(pos);
}


void UJoint::CalcVelocity()
{

	float angle = ConstraintInstance.GetCurrentTwist();
	double time = FPlatformTime::Seconds();

	float deltatime = time - oldTime;
	
	velocity = angle - oldAngle;

	if (abs(velocity) > PI) 
		velocity -= (signbit(velocity) ? -1 : 1) * 2 * PI;

	position = oldPosition + velocity;

	velocity /= deltatime;
	
	oldAngle = angle;
	oldTime = time;
	oldPosition = position;
}

float UJoint::GetAngularVelocity()
{
	return velocity;
}

void UJoint::SetAngularVelocity(float value)
{
	ConstraintInstance.SetOrientationDriveTwistAndSwing(false, false);
	ConstraintInstance.SetAngularVelocityTarget(FVector(value/(2 * PI), 0, 0));
}

float UJoint::GetEffort()
{
	FVector linear, angular;

	ConstraintInstance.GetConstraintForce(linear, angular);
	//UE_LOG(LogTemp, Error, TEXT("%s"), *angular.ToString());
	return angular.X / 100000; // because UE uses cm instead of m
}

UPrimitiveComponent* UJoint::GetComponentInternal(EConstraintFrame::Type Frame) const
{
	UPrimitiveComponent* PrimComp = NULL;

	FName ComponentName = NAME_None;
	AActor* Actor = NULL;

	// Frame 1
	if (Frame == EConstraintFrame::Frame1)
	{
		// Use override component if specified
		if (OverrideComponent1.IsValid())
		{
			return OverrideComponent1.Get();
		}

		ComponentName = ComponentName1.ComponentName;
		Actor = ConstraintActor1;
	}
	// Frame 2
	else
	{
		// Use override component if specified
		if (OverrideComponent2.IsValid())
		{
			return OverrideComponent2.Get();
		}

		ComponentName = ComponentName2.ComponentName;
		Actor = ConstraintActor2;
	}

	// If neither actor nor component name specified, joint to 'world'
	if (Actor != NULL || ComponentName != NAME_None)
	{
		// If no actor specified, but component name is - use Owner
		if (Actor == NULL)
		{
			Actor = GetOwner();
		}

		// If we now have an Actor, lets find a component
		if (Actor != NULL)
		{
			// No name specified, use the root component
			if (ComponentName == NAME_None)
			{
				PrimComp = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
			}
			// Name specified, see if we can find that component..
			else
			{
				for (UActorComponent* Comp : Actor->GetComponents())
				{
					if (Comp->GetFName() == ComponentName)
					{
						if (UChildActorComponent* ChildActorComp = Cast<UChildActorComponent>(Comp))
						{
							if (AActor* ChildActor = ChildActorComp->GetChildActor())
							{
								PrimComp = Cast<UPrimitiveComponent>(ChildActor->GetRootComponent());
							}
						}
						else
						{
							PrimComp = Cast<UPrimitiveComponent>(Comp);
						}
						break;
					}
				}
			}
		}
	}

	return PrimComp;
}

//Helps to find bone index if bone specified, otherwise find bone index of root body
int32 GetBoneIndexHelper(FName InBoneName, const USkeletalMeshComponent& SkelComp, int32* BodyIndex = nullptr)
{
	FName BoneName = InBoneName;
	const UPhysicsAsset* PhysAsset = SkelComp.GetPhysicsAsset();

	if (BoneName == NAME_None)
	{
		//Didn't specify bone name so just use root body
		if (PhysAsset)
		{
			const int32 RootBodyIndex = SkelComp.FindRootBodyIndex();
			if (PhysAsset->SkeletalBodySetups.IsValidIndex(RootBodyIndex))
			{
				BoneName = PhysAsset->SkeletalBodySetups[RootBodyIndex]->BoneName;
			}
		}
	}

	if (BodyIndex)
	{
		*BodyIndex = PhysAsset ? PhysAsset->FindBodyIndex(BoneName) : INDEX_NONE;
	}

	return SkelComp.GetBoneIndex(BoneName);
}

FTransform UJoint::GetBodyTransformInternal(EConstraintFrame::Type Frame, FName InBoneName) const
{
	UPrimitiveComponent* PrimComp = GetComponentInternal(Frame);
	if (!PrimComp)
	{
		return FTransform::Identity;
	}

	//Use GetComponentTransform() by default for all components
	FTransform ResultTM = PrimComp->GetComponentTransform();

	// Skeletal case
	if (const USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(PrimComp))
	{
		const int32 BoneIndex = GetBoneIndexHelper(InBoneName, *SkelComp);
		if (BoneIndex != INDEX_NONE)
		{
			ResultTM = SkelComp->GetBoneTransform(BoneIndex);
		}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		else
		{
			FMessageLog("PIE").Warning(FText::Format(LOCTEXT("BadBoneNameToConstraint", "Couldn't find bone {0} for ConstraintComponent {1}."),
				FText::FromName(InBoneName), FText::FromString(GetPathNameSafe(this))));
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	}

	return ResultTM;
}

FBox UJoint::GetBodyBoxInternal(EConstraintFrame::Type Frame, FName InBoneName) const
{
	FBox ResultBox(ForceInit);

	UPrimitiveComponent* PrimComp = GetComponentInternal(Frame);

	// Skeletal case
	if (const USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(PrimComp))
	{
		if (const UPhysicsAsset* PhysicsAsset = SkelComp->GetPhysicsAsset())
		{
			int32 BodyIndex;
			const int32 BoneIndex = GetBoneIndexHelper(InBoneName, *SkelComp, &BodyIndex);
			if (BoneIndex != INDEX_NONE && BodyIndex != INDEX_NONE)
			{
				const FTransform BoneTransform = SkelComp->GetBoneTransform(BoneIndex);
				ResultBox = PhysicsAsset->SkeletalBodySetups[BodyIndex]->AggGeom.CalcAABB(BoneTransform);
			}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			else
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("BadBoneNameToConstraint2", "Couldn't find bone {0} for ConstraintComponent {1}."),
					FText::FromName(InBoneName), FText::FromString(GetPathNameSafe(this))));
			}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		}
	}
	else if (PrimComp != NULL)
	{
		ResultBox = PrimComp->Bounds.GetBox();
	}

	return ResultBox;
}

FTransform UJoint::GetBodyTransform(EConstraintFrame::Type Frame) const
{
	if (Frame == EConstraintFrame::Frame1)
	{
		return GetBodyTransformInternal(Frame, ConstraintInstance.ConstraintBone1);
	}
	else
	{
		return GetBodyTransformInternal(Frame, ConstraintInstance.ConstraintBone2);
	}
}

FBox UJoint::GetBodyBox(EConstraintFrame::Type Frame) const
{
	if (Frame == EConstraintFrame::Frame1)
	{
		return GetBodyBoxInternal(Frame, ConstraintInstance.ConstraintBone1);
	}
	else
	{
		return GetBodyBoxInternal(Frame, ConstraintInstance.ConstraintBone2);
	}
}

FBodyInstance* UJoint::GetBodyInstance(EConstraintFrame::Type Frame) const
{
	FBodyInstance* Instance = NULL;
	UPrimitiveComponent* PrimComp = GetComponentInternal(Frame);
	if (PrimComp != NULL)
	{
		if (Frame == EConstraintFrame::Frame1)
		{
			Instance = PrimComp->GetBodyInstance(ConstraintInstance.ConstraintBone1);
		}
		else
		{
			Instance = PrimComp->GetBodyInstance(ConstraintInstance.ConstraintBone2);
		}
	}
	return Instance;
}


/** Wrapper that calls our constraint broken delegate */
void UJoint::OnConstraintBrokenWrapper(int32 ConstraintIndex)
{
	OnConstraintBroken.Broadcast(ConstraintIndex);
}

void UJoint::InitComponentConstraint()
{
	// First we convert world space position of constraint into local space frames
	UpdateConstraintFrames();

	// Then we init the constraint
	FBodyInstance* Body1 = GetBodyInstance(EConstraintFrame::Frame1);
	FBodyInstance* Body2 = GetBodyInstance(EConstraintFrame::Frame2);

	if (Body1 != nullptr || Body2 != nullptr)
	{
		ConstraintInstance.InitConstraint(Body1, Body2, GetConstraintScale(), this, FOnConstraintBroken::CreateUObject(this, &UJoint::OnConstraintBrokenWrapper));
	}
}

void UJoint::TermComponentConstraint()
{
	ConstraintInstance.TermConstraint();
}

void UJoint::OnConstraintBrokenHandler(FConstraintInstance* BrokenConstraint)
{
	OnConstraintBroken.Broadcast(BrokenConstraint->ConstraintIndex);
}

float UJoint::GetConstraintScale() const
{
	return GetComponentScale().GetAbsMin();
}

void UJoint::SetConstrainedComponents(UPrimitiveComponent* Component1, FName BoneName1, UPrimitiveComponent* Component2, FName BoneName2)
{
	if (Component1 != NULL)
	{
		this->ComponentName1.ComponentName = Component1->GetFName();
		OverrideComponent1 = Component1;
		ConstraintInstance.ConstraintBone1 = BoneName1;
	}

	if (Component2 != NULL)
	{
		this->ComponentName2.ComponentName = Component2->GetFName();
		OverrideComponent2 = Component2;
		ConstraintInstance.ConstraintBone2 = BoneName2;
	}

	InitComponentConstraint();
}

void UJoint::BreakConstraint()
{
	ConstraintInstance.TermConstraint();
}


void UJoint::InitializeComponent()
{
	Super::InitializeComponent();
	InitComponentConstraint();
}

#if WITH_EDITOR
void UJoint::OnRegister()
{
	Super::OnRegister();

	if (SpriteComponent)
	{
		UpdateSpriteTexture();
		SpriteComponent->SpriteInfo.Category = TEXT("Physics");
		SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("SpriteCategory", "Physics", "Physics");
	}
}
#endif

void UJoint::OnUnregister()
{
	Super::OnUnregister();

	// #PHYS2 This is broken because if you re-register again, constraint will not be created, as constraint is init'd ini InitializeComponent not RegisterComponent
	TermComponentConstraint();
}

void UJoint::BeginDestroy()
{
	Super::BeginDestroy();

	// Should not be destroying a constraint component with the constraint still created
	ensure(ConstraintInstance.IsTerminated());
}

void UJoint::PostLoad()
{
	Super::PostLoad();

	// Fix old content that used a ConstraintSetup
	if (GetLinkerUE4Version() < VER_UE4_ALL_PROPS_TO_CONSTRAINTINSTANCE && (ConstraintSetup_DEPRECATED != NULL))
	{
		// Will have copied from setup into DefaultIntance inside
		ConstraintInstance.CopyConstraintParamsFrom(&ConstraintSetup_DEPRECATED->DefaultInstance);
		ConstraintSetup_DEPRECATED = NULL;
	}

	if (GetLinkerUE4Version() < VER_UE4_SOFT_CONSTRAINTS_USE_MASS)
	{
		//In previous versions the mass was placed into the spring constant. This is correct because you use different springs for different mass - however, this makes tuning hard
		//We now multiply mass into the spring constant. To fix old data we use CalculateMass which is not perfect but close (within 0.1kg)
		//We also use the primitive body instance directly for determining if simulated - this is potentially wrong for fixed bones in skeletal mesh, but it's much more likely right (in skeletal case we don't have access to bodies to check)

		UPrimitiveComponent * Primitive1 = GetComponentInternal(EConstraintFrame::Frame1);
		UPrimitiveComponent * Primitive2 = GetComponentInternal(EConstraintFrame::Frame2);

		int NumDynamic = 0;
		float TotalMass = 0.f;

		if (Primitive1 && Primitive1->BodyInstance.bSimulatePhysics)
		{
			FName BoneName = ConstraintInstance.ConstraintBone1;
			++NumDynamic;
			TotalMass += Primitive1->CalculateMass(BoneName);
		}

		if (Primitive2 && Primitive2->BodyInstance.bSimulatePhysics)
		{
			FName BoneName = ConstraintInstance.ConstraintBone2;
			++NumDynamic;
			TotalMass += Primitive2->CalculateMass(BoneName);
		}

		if ((NumDynamic > 0) && (TotalMass > 0))	//we don't support cases where both constrained bodies are static or NULL, but add this anyway to avoid crash
		{
			float AverageMass = TotalMass / NumDynamic;

#if WITH_EDITORONLY_DATA
			ConstraintInstance.ProfileInstance.LinearLimit.Stiffness /= AverageMass;
			ConstraintInstance.SwingLimitStiffness_DEPRECATED /= AverageMass;
			ConstraintInstance.TwistLimitStiffness_DEPRECATED /= AverageMass;
			ConstraintInstance.LinearLimitDamping_DEPRECATED /= AverageMass;
			ConstraintInstance.SwingLimitDamping_DEPRECATED /= AverageMass;
			ConstraintInstance.TwistLimitDamping_DEPRECATED /= AverageMass;

#endif
		}

	}
}

#if WITH_EDITOR
void UJoint::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	ConstraintInstance.ProfileInstance.SyncChangedConstraintProperties(PropertyChangedEvent);
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

void UJoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateConstraintFrames();
	UpdateSpriteTexture();
}

void UJoint::PostEditComponentMove(bool bFinished)
{
	Super::PostEditComponentMove(bFinished);

	// Update frames
	UpdateConstraintFrames();
}

void UJoint::CheckForErrors()
{
	Super::CheckForErrors();

	UPrimitiveComponent* PrimComp1 = GetComponentInternal(EConstraintFrame::Frame1);
	UPrimitiveComponent* PrimComp2 = GetComponentInternal(EConstraintFrame::Frame2);

	// Check we have something to joint
	if (PrimComp1 == NULL && PrimComp2 == NULL)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("OwnerName"), FText::FromString(GetNameSafe(GetOwner())));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("NoComponentsFound", "{OwnerName} : No components found to joint."), Arguments)));
	}
	// Make sure constraint components are not both static.
	else if (PrimComp1 != NULL && PrimComp2 != NULL)
	{
		if (PrimComp1->Mobility != EComponentMobility::Movable && PrimComp2->Mobility != EComponentMobility::Movable)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("OwnerName"), FText::FromString(GetNameSafe(GetOwner())));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT("BothComponentsStatic", "{OwnerName} : Both components are static."), Arguments)));
		}
	}
	else
	{
		// At this point, we know one constraint component is NULL and the other is non-NULL.
		// Check that the non-NULL constraint component is dynamic.
		if ((PrimComp1 == NULL && PrimComp2->Mobility != EComponentMobility::Movable) ||
			(PrimComp2 == NULL && PrimComp1->Mobility != EComponentMobility::Movable))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("OwnerName"), FText::FromString(GetNameSafe(GetOwner())));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT("SingleStaticComponent", "{OwnerName} : Connected to single static component."), Arguments)));
		}
	}
}

#endif // WITH_EDITOR

void UJoint::UpdateConstraintFrames()
{
	FTransform A1Transform = GetBodyTransform(EConstraintFrame::Frame1);
	A1Transform.RemoveScaling();

	FTransform A2Transform = GetBodyTransform(EConstraintFrame::Frame2);
	A2Transform.RemoveScaling();

	// World ref frame
	const FVector WPos = GetComponentLocation();
	const FVector WPri = GetComponentTransform().GetUnitAxis(EAxis::X);
	const FVector WOrth = GetComponentTransform().GetUnitAxis(EAxis::Y);

	ConstraintInstance.Pos1 = A1Transform.InverseTransformPosition(WPos);
	ConstraintInstance.PriAxis1 = A1Transform.InverseTransformVectorNoScale(WPri);
	ConstraintInstance.SecAxis1 = A1Transform.InverseTransformVectorNoScale(WOrth);

	const FVector RotatedX = ConstraintInstance.AngularRotationOffset.RotateVector(FVector(1, 0, 0));
	const FVector RotatedY = ConstraintInstance.AngularRotationOffset.RotateVector(FVector(0, 1, 0));
	const FVector WPri2 = GetComponentTransform().TransformVectorNoScale(RotatedX);
	const FVector WOrth2 = GetComponentTransform().TransformVectorNoScale(RotatedY);


	ConstraintInstance.Pos2 = A2Transform.InverseTransformPosition(WPos);
	ConstraintInstance.PriAxis2 = A2Transform.InverseTransformVectorNoScale(WPri2);
	ConstraintInstance.SecAxis2 = A2Transform.InverseTransformVectorNoScale(WOrth2);

	//Constraint instance is given our reference frame scale and uses it to scale position.
	//Note that the scale passed in is also used for limits, so we first undo the position scale so that it's consistent.

	//Note that in the case where there is no body instance, the position is given in world space and there is no scaling.
	const float RefScale = FMath::Max(GetConstraintScale(), 0.01f);
	if (GetBodyInstance(EConstraintFrame::Frame1))
	{
		ConstraintInstance.Pos1 /= RefScale;
	}

	if (GetBodyInstance(EConstraintFrame::Frame2))
	{
		ConstraintInstance.Pos2 /= RefScale;
	}
}

#if WITH_EDITOR
void UJoint::UpdateSpriteTexture()
{
	if (SpriteComponent)
	{
		if (ConstraintUtils::IsHinge(ConstraintInstance))
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/S_KHinge.S_KHinge")));
		}
		else if (ConstraintUtils::IsPrismatic(ConstraintInstance))
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/S_KPrismatic.S_KPrismatic")));
		}
		else
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/S_KBSJoint.S_KBSJoint")));
		}
	}
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
