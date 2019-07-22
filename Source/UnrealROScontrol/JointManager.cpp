// Fill out your copyright notice in the Description page of Project Settings.


#include "JointManager.h"

// Sets default values
AJointManager::AJointManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void AJointManager::Subscribe(UJoint *joint)
{
	Joints.Emplace(joint->Label, joint);
	UE_LOG(LogTemp, Warning, TEXT("Joint subscribed %s"), *joint->Label);

	for (auto& Elem : Joints)
	{
		UE_LOG(LogTemp, Error, TEXT("__ %s __\n"), *Elem.Key);
	}
}

void AJointManager::Unsubscribe(UJoint *joint)
{
	Joints.Remove(joint->Label);
}

// Called when the game starts or when spawned
void AJointManager::BeginPlay()
{
	Super::BeginPlay();
	
	Connect();

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(TimerHandle, this, &AJointManager::SendUpdate, 0.02f, true);
	}
}

void AJointManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	Disconnect();
}

// Called every frame
void AJointManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	uint32 size;
	int32 bytesRead;
	if (Socket->HasPendingData(size))
	{
		Socket->Recv(buffer, 8, bytesRead);

		temp = ntohll(*(uint64_t *)buffer);

		double value = *(double *)&temp;

		Joints["joint1"]->SetAngle(value);
		UE_LOG(LogTemp, Error, TEXT("%f"), value);
	}
}


void AJointManager::Connect() {
	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("default"), false);

	FIPv4Address ip(127, 0, 0, 1);

	TSharedRef<FInternetAddr> addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	addr->SetIp(ip.Value);
	addr->SetPort(8080);

	bool connected = Socket->Connect(*addr);
	UE_LOG(LogTemp, Warning, TEXT("START SOCKET! Connected? %s"), (connected ? TEXT("True") : TEXT("False")));
}

void AJointManager::Disconnect() {
	bool status = Socket->Close();
	UE_LOG(LogTemp, Warning, TEXT("CLSOE SOCKET! Sucessfull? %s"), (status ? TEXT("True") : TEXT("False")));
}



void AJointManager::SendUpdate()
{


	double twist = Joints["joint1"]->GetAngle();

	uint8_t *pointer = buffer;

	*(uint16_t *)pointer = htons(1);
	pointer += 2;

	char * name = "joint1";
	uint16_t length = strlen(name) + 1;
	*(uint16_t *)pointer = htons(length);
	pointer += 2;

	strcpy_s((char *)pointer, 1024, name);
	pointer += length;

	temp = htonll(*(uint64_t *)&twist);
	*(uint64_t *)pointer = temp;
	pointer += 4;

	*(uint64_t *)pointer = temp;
	pointer += 4;

	*(uint64_t *)pointer = temp;
	pointer += 4;

	int32 sent;

	bool status = Socket->Send(buffer, pointer - buffer, sent);
	//if (!status) Disconnect();
	//ESocketConnectionState state = Socket->GetConnectionState();
	//if (state != SCS_Connected) Connect();
}


