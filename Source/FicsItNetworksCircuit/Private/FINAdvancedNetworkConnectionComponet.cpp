#include "FINAdvancedNetworkConnectionComponent.h"
#include "FicsItNetworksCircuit.h"
#include "FINNetworkCircuit.h"
#include "FGBlueprintSubsystem.h"
#include "Buildables/FGBuildable.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void UFINAdvancedNetworkConnectionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UFINAdvancedNetworkConnectionComponent, ID);
	DOREPLIFETIME(UFINAdvancedNetworkConnectionComponent, Nick);
}

UFINAdvancedNetworkConnectionComponent::UFINAdvancedNetworkConnectionComponent() : RedirectionObject(this) {
	SetIsReplicatedByDefault(true);
}

void UFINAdvancedNetworkConnectionComponent::BeginPlay() {
	Super::BeginPlay();

	AFGBuildable* Buildable = GetOwner<AFGBuildable>();
	if (!Buildable || !Buildable->GetBlueprintDesigner()) {
		if (bOuterAsRedirect) RedirectionObject = GetOuter();

		if (GetOwner()->HasAuthority()) {
			// An invalid (all-zero) ID means either the ID was never created,
			// or the saved ID got lost/corrupted. Both cases have to be recovered from,
			// otherwise every affected component would share the same invalid ID
			// and could no longer be addressed individually over the network.
			if (!bIdCreated || !ID.IsValid()) {
				if (bIdCreated) {
					UE_LOG(LogFicsItNetworksCircuit, Warning, TEXT("Network Component '%s' of '%s' had an invalid ID stored, a new ID gets generated."), *GetName(), *GetOwner()->GetName());
				}
				ID = FGuid::NewGuid();
				bIdCreated = true;
			}

			// setup circuit
			if (!Circuit) {
				Circuit = GetWorld()->SpawnActor<AFINNetworkCircuit>();
				Circuit->Recalculate(this);
			}
		}
	}
}

void UFINAdvancedNetworkConnectionComponent::Serialize(FArchive& Ar) {
	Super::Serialize(Ar);
}

bool UFINAdvancedNetworkConnectionComponent::ShouldSave_Implementation() const {
	return true;
}

void UFINAdvancedNetworkConnectionComponent::NotifyNetworkUpdate_Implementation(int Type, const TSet<UObject*>& Nodes) {
	for (UObject* Node : Nodes) {
		if (Node->GetClass()->ImplementsInterface(UFINNetworkComponent::StaticClass())) {
			netSig_NetworkUpdate(Type, IFINNetworkComponent::Execute_GetID(Node).ToString());
		}
	}
}

FGuid UFINAdvancedNetworkConnectionComponent::GetID_Implementation() const {
	return ID;
}

FString UFINAdvancedNetworkConnectionComponent::GetNick_Implementation() const {
	return Nick;
}

void UFINAdvancedNetworkConnectionComponent::SetNick_Implementation(const FString& NewNick) {
	Nick = NewNick;
	GetOwner()->ForceNetUpdate();
}

bool UFINAdvancedNetworkConnectionComponent::HasNick_Implementation(const FString& inNick) {
	return HasNickByNick(inNick, Execute_GetNick(this));
}

UObject* UFINAdvancedNetworkConnectionComponent::GetInstanceRedirect_Implementation() {
	return RedirectionObject;
}

bool UFINAdvancedNetworkConnectionComponent::AccessPermitted_Implementation(FGuid inID) const {
	return true;
}

UObject* UFINAdvancedNetworkConnectionComponent::GetSignalSenderOverride_Implementation() {
	return this;
}

void UFINAdvancedNetworkConnectionComponent::HandleSignal(const FFINSignalData& Signal, const FFIRTrace& Sender) {
	OnNetworkSignal.Broadcast(Signal, Sender);
}

bool UFINAdvancedNetworkConnectionComponent::IsPortOpen(int Port) {
	if (OnIsNetworkPortOpen.IsBound()) {
		return OnIsNetworkPortOpen.Execute(Port);
	}
	return false;
}

void UFINAdvancedNetworkConnectionComponent::HandleMessage(const FGuid& InID, const FGuid& Sender, const FGuid& Receiver, int Port, const TArray<FFIRAnyValue>& Data) {
	OnNetworkMessageRecieved.Broadcast(InID, Sender, Receiver, Port, Data);
}

bool UFINAdvancedNetworkConnectionComponent::IsNetworkMessageRouter() const {
	if (OnIsNetworkRouter.IsBound()) {
		return OnIsNetworkRouter.Execute();
	}
	return false;
}
