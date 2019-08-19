// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Joint.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Networking.h"
#include "JointManager.generated.h"


#ifdef _WIN32
#if PLATFORM_LITTLE_ENDIAN
#define htons(n) ((((unsigned short)(n) & 0xFF00) >> 8) | (((unsigned short)(n) & 0xFF) << 8))
#define ntohs(n) ((((unsigned short)(n) & 0xFF00) >> 8) | (((unsigned short)(n) & 0xFF) << 8))

#define htonl(n) (((((unsigned long)(n) & 0xFF)) << 24) | \
                  ((((unsigned long)(n) & 0xFF00)) << 8) | \
                  ((((unsigned long)(n) & 0xFF0000)) >> 8) | \
                  ((((unsigned long)(n) & 0xFF000000)) >> 24))

#define ntohl(n) (((((unsigned long)(n) & 0xFF)) << 24) | \
                  ((((unsigned long)(n) & 0xFF00)) << 8) | \
                  ((((unsigned long)(n) & 0xFF0000)) >> 8) | \
                  ((((unsigned long)(n) & 0xFF000000)) >> 24))

# define htonll(x) (((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
# define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#else
# define htonll(x) (x)
# define ntohll(x) (x)
#endif

#elif __unix__
#include <arpa/inet.h>

#if __BIG_ENDIAN__
# define htonll(x) (x)
# define ntohll(x) (x)
#else
# define htonll(x) (((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
# define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif

#endif


class RecieveTask : public FNonAbandonableTask
{
private:
	FSocket *Socket;
	uint8_t buffer[1024];
	TMap<FString, UJoint *> *Joints;

	volatile bool bRun = true;
public:
	UPROPERTY()
	RecieveTask(FSocket* Socket, TMap<FString, UJoint *> *Joints, volatile bool **bRun);

	~RecieveTask();

	// Required by UE4
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(RecieveTask, STATGROUP_ThreadPoolAsyncTasks)
	}

	void DoWork();
};

UCLASS()
class UNREALROSCONTROL_API AJointManager : public AActor
{
	GENERATED_BODY()
	
private:
	TMap<FString, UJoint *> Joints;

	FSocket* Socket;
	double last_twist;
	uint8_t buffer[1024];
	uint64_t temp;

	FTimerHandle TimerHandle;
	FAutoDeleteAsyncTask<RecieveTask> *RecieveTaskHandle;
	volatile bool *bRun;

	UFUNCTION()
		void SendUpdate();

	void Connect();
	void Disconnect();

public:	
	// Sets default values for this actor's properties
	AJointManager();

	void Subscribe(UJoint *joint);
	void Unsubscribe(UJoint *joint);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};

