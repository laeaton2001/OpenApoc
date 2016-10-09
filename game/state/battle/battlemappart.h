#pragma once

#include "game/state/stateobject.h"
#include "library/sp.h"
#include "library/vec.h"
#include <list>

#define TICKS_PER_FRAME_MAP_PART 8

namespace OpenApoc
{
class Battle;
class Collision;
class TileObjectBattleMapPart;
class BattleMapPartType;
class BattleItem;
class BattleDoor;
class BattleMap;

class BattleMapPart : public std::enable_shared_from_this<BattleMapPart>
{
  public:
	StateRef<BattleMapPartType> type;
	StateRef<BattleMapPartType> alternative_type;

	Vec3<float> getPosition() const { return currentPosition; }

	Vec3<int> initialPosition;
	Vec3<float> currentPosition;

	unsigned int ticksUntilTryCollapse = 0;
	bool damaged = false;
	bool falling = false;
	bool destroyed = false;
	StateRef<BattleDoor> door;

	// Ticks for animation of non-doors
	int animation_frame_ticks = 0;
	int getAnimationFrame();
	int getMaxFrames();

	// Returns true if sound and doodad were handled by it
	bool handleCollision(GameState &state, Collision &c);
	// Check if we are still supported, and collapse if not
	void tryCollapse();

	void ceaseDoorFunction();

	void update(GameState &state, unsigned int ticks);
	// Queue attempt to collapse
	void queueTryCollapse();

	bool isAlive() const;

	~BattleMapPart() = default;

	// Following members are not serialized, but rather are set in initBattle method

	sp<TileObjectBattleMapPart> tileObject;
	std::list<wp<BattleItem>> supportedItems;
	std::list<wp<BattleMapPart>> supportedParts;

  private:
	// Find map parts that support this one and set "supported" flag
	bool findSupport();
	// Cease providing or requiring support
	void ceaseSupportProvision();
};
}
