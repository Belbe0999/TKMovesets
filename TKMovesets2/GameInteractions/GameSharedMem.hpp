#pragma once

#include "Online.hpp"
#include "GameImport.hpp"

struct onlineImportEntry
{
	movesetInfo moveset;
	ImportSettings settings;
	unsigned int playerId;
};

class GameSharedMem : public GameImport
{
private:
	// Movesets (data, size) to import and corresponding player address
	std::vector<onlineImportEntry> m_plannedImportations;
	// If true, will order the MovesetLoader.dll to be injected on the next RunningUpdate, in another thread
	bool m_requestedInjection = false;
	// Online class, manages shared memory
	Online* m_sharedMemHandler = nullptr;
	// Online class that is going to be freed next FreeExpiredFactoryClasses() call
	Online* m_toFree_sharedMemHandler = nullptr;

	// Callback called whenever the process is re-attached
	void OnProcessAttach() override;
	// Callback called whenever the process is detached
	void OnProcessDetach() override;
	// Instantiate an extractor with polymorphism, also destroy the old one
	void InstantiateFactory() override;
	// Function ran in the parallel thread, used to latch on to process and load the shared memory
	void RunningUpdate() override;

public:
	// Copy of sharedMemHandler.displayedMovesets in case it gets deallocated
	std::vector<movesetInfo> displayedMovesets;
	// True if we are currently injecting the MovesetLoader
	bool isInjecting = false;
	// True if the shared memory of .displayedMovesets is currently loaded
	bool isMemoryLoaded = false;
	// True if the DLL was successfully injected
	bool isInjected = false;
	// True if we are locked in
	bool lockedIn = false;

	// Default flags are read-only, so the importer needs this
	GameSharedMem() { m_processExtraFlags = PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD; }
	// Always true because SharedMem can function as long as the process is started. Condition used to trigger RunningUpdate()
	bool CanStart(bool cached=true) override { return true; };
	// Is currently busy with an importation
	bool IsBusy() override;
	// Stops the thread started above
	void StopThreadAndCleanup() override;
	// Frees the factory-obtained class we allocated earlier
	void FreeExpiredFactoryClasses() override;

	// Queue a DLL injection for the next RunningUpdate() tick
	void InjectDll();

	void QueueCharacterImportation(movesetInfo* moveset, unsigned int playerId, ImportSettings settings = 0);

	// Forbid access to these
	void QueueCharacterImportation(std::wstring filename, ImportSettings settings = 0) override { throw; };
	void QueueCharacterImportation(const Byte* moveset, uint64_t movesetSize, ImportSettings settings = 0) override { throw; };
};
