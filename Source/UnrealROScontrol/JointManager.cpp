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

	//for (auto& Elem : Joints)
	//{
	//	UE_LOG(LogTemp, Error, TEXT("__ %s __\n"), *Elem.Key);
	//}
}

void AJointManager::Unsubscribe(UJoint *joint)
{
	Joints.Remove(joint->Label);
}

// Called when the game starts or when spawned
void AJointManager::BeginPlay()
{
	Super::BeginPlay();
	
	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("default"), false);

	Connect();

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(TimerHandle, this, &AJointManager::SendUpdate, 0.02f, true);
	}

	RecieveTaskHandle = new FAutoDeleteAsyncTask<RecieveTask>(Socket, &Joints, &bRun);
	RecieveTaskHandle->StartBackgroundTask();
}

void AJointManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	Disconnect();
	*bRun = false;
}

// Called every frame
void AJointManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


void AJointManager::Connect() {
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
	uint8_t *pointer = buffer;

	*(uint16_t *)pointer = htons(Joints.Num());
	pointer += 2;

	for (auto& Elem : Joints)
	{
		UJoint *joint = Elem.Value;

		double angle = joint->GetAngle();
		double velocity = joint->GetAngularVelocity();
		double effort = joint->GetEffort();

		char * name = TCHAR_TO_ANSI(*joint->Label);
		uint16_t length = strlen(name) + 1;
		*(uint16_t *)pointer = htons(length);
		pointer += 2;

		strcpy_s((char *)pointer, 1024, name);
		pointer += length;

		temp = htonll(*(uint64_t *)&angle);
		*(uint64_t *)pointer = temp;
		pointer += 8;

		temp = htonll(*(uint64_t *)&velocity);
		*(uint64_t *)pointer = temp;
		pointer += 8;

		temp = htonll(*(uint64_t *)&effort);
		*(uint64_t *)pointer = temp;
		pointer += 8;
	}

	int32 sent;
	bool status = Socket->Send(buffer, pointer - buffer, sent);
	//if (!status) Disconnect();
	//ESocketConnectionState state = Socket->GetConnectionState();
	//if (state != SCS_Connected) Connect();
}


RecieveTask::RecieveTask(FSocket *Socket, TMap<FString, UJoint *> *Joints,volatile bool **bRun)
{
	this->Socket = Socket;
	this->Joints = Joints;
	*bRun = &this->bRun;
}

RecieveTask::~RecieveTask()
{
}

void RecieveTask::DoWork()
{
	int32 bytesRead;
	uint64_t temp;

	while (bRun)
	{
		// read number of joints		
		Socket->Recv(buffer, 2, bytesRead, ESocketReceiveFlags::Type::WaitAll);
		uint16_t nrJoints = ntohs(*(uint16_t *)buffer);

		//UE_LOG(LogTemp, Error, TEXT("test1, %d, %d"), nrJoints, bytesRead);
		for (int i = 0; i < nrJoints; i++)
		{
			//UE_LOG(LogTemp, Error, TEXT("test2"));
			// read lenght  of label
			Socket->Recv(buffer, 2, bytesRead, ESocketReceiveFlags::Type::WaitAll);
			uint16_t labelLenght = ntohs(*(uint16_t *)buffer);

			//UE_LOG(LogTemp, Error, TEXT("test3, %d, %d"), labelLenght, bytesRead);
			// read label
			Socket->Recv(buffer, labelLenght, bytesRead, ESocketReceiveFlags::Type::WaitAll);
			FString label = FString(ANSI_TO_TCHAR((char *)buffer));

			//UE_LOG(LogTemp, Error, TEXT("test4, %d"), bytesRead);
			// read command
			Socket->Recv(buffer, 8, bytesRead, ESocketReceiveFlags::Type::WaitAll);
			temp = ntohll(*(uint64_t *)buffer);

			//UE_LOG(LogTemp, Error, TEXT("test5, %d"), bytesRead);
			double value = *(double *)&temp;
			
			//UE_LOG(LogTemp, Error, TEXT("test6, %d"), value);
			if (!bRun) return;
			if ((*Joints).Contains(label))
			{
				(*Joints)[label]->ExecuteCommand(value);
			}
			//UE_LOG(LogTemp, Error, TEXT("%s: %f"), *label, value);
		}
	}
}