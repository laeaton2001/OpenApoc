#include "game/ui/tileview/tileview.h"
#include "framework/event.h"
#include "framework/framework.h"
#include "framework/includes.h"
#include "game/state/battle.h"

namespace OpenApoc
{

TileView::TileView(TileMap &map, Vec3<int> isoTileSize, Vec2<int> stratTileSize,
                   TileViewMode initialMode, Mode mode)
    : Stage(), map(map), isoTileSize(isoTileSize), stratTileSize(stratTileSize),
      viewMode(initialMode), mode(mode), scrollUp(false), scrollDown(false), scrollLeft(false),
      scrollRight(false), dpySize(fw().displayGetWidth(), fw().displayGetHeight()),
      strategyViewBoxColour(212, 176, 172, 255), strategyViewBoxThickness(2.0f), currentZLevel(1),
      selectedTilePosition(0, 0, 0), selectedTileImageOffset(0, 0), maxZDraw(map.size.z),
      centerPos(0, 0, 0), isoScrollSpeed(0.5, 0.5), stratScrollSpeed(2.0f, 2.0f)
{
	switch (mode)
	{
		case Mode::City:
			layerDrawingMode = LayerDrawingMode::AllLevels;
			selectedTileEmptyImageBack = fw().data->loadImage("city/selected-citytile-back.png");
			selectedTileFilledImageBack = fw().data->loadImage("city/selected-citytile-back.png");
			selectedTileBackgroundImageBack = fw().data->loadImage("city/selected-citytile-back.png");
			selectedTileEmptyImageFront = fw().data->loadImage("city/selected-citytile-front.png");
			selectedTileFilledImageFront = fw().data->loadImage("city/selected-citytile-front.png");
			selectedTileBackgroundImageFront = fw().data->loadImage("city/selected-citytile-front.png");
			pal = fw().data->loadPalette("xcom3/ufodata/pal_01.dat");
			break;
		case Mode::Battle:
			layerDrawingMode = LayerDrawingMode::UpToCurrentLevel;
			selectedTileEmptyImageBack =
			    fw().data->loadImage("battle/selected-battletile-empty-back.png");
			selectedTileEmptyImageFront =
			    fw().data->loadImage("battle/selected-battletile-empty-front.png");
			selectedTileFilledImageBack =
			    fw().data->loadImage("battle/selected-battletile-filled-back.png");
			selectedTileFilledImageFront =
			    fw().data->loadImage("battle/selected-battletile-filled-front.png");
			selectedTileBackgroundImageBack =
				fw().data->loadImage("battle/selected-battletile-background-back.png");
			selectedTileBackgroundImageFront =
				fw().data->loadImage("battle/selected-battletile-background-front.png");
			selectedTileImageOffset = {23, 22};
			pal = fw().data->loadPalette("xcom3/tacdata/tactical.pal");
			break;
		default:
			LogError("Unknown TileView::Mode %d", (int)mode);
			break;
	}

	LogInfo("dpySize: {%d,%d}", dpySize.x, dpySize.y);
}

TileView::~TileView() = default;

void TileView::begin() {}

void TileView::pause() {}

void TileView::resume() {}

void TileView::finish() {}

void TileView::eventOccurred(Event *e)
{
	if (e->type() == EVENT_KEY_DOWN)
	{
		switch (e->keyboard().KeyCode)
		{
			case SDLK_UP:
				scrollUp = true;
				break;
			case SDLK_DOWN:
				scrollDown = true;
				break;
			case SDLK_LEFT:
				scrollLeft = true;
				break;
			case SDLK_RIGHT:
				scrollRight = true;
				break;
			case SDLK_s:
				if (selectedTilePosition.y < (map.size.y - 1))
					selectedTilePosition.y++;
				break;
			case SDLK_w:
				if (selectedTilePosition.y > 0)
					selectedTilePosition.y--;
				break;
			case SDLK_a:
				if (selectedTilePosition.x > 0)
					selectedTilePosition.x--;
				break;
			case SDLK_d:
				if (selectedTilePosition.x < (map.size.x - 1))
					selectedTilePosition.x++;
				break;
			case SDLK_r:
				if (selectedTilePosition.z < (map.size.z - 1))
					selectedTilePosition.z++;
				break;
			case SDLK_f:
				if (selectedTilePosition.z > 0)
					selectedTilePosition.z--;
				break;
			case SDLK_1:
				pal = fw().data->loadPalette("xcom3/ufodata/pal_01.dat");
				break;
			case SDLK_2:
				pal = fw().data->loadPalette("xcom3/ufodata/pal_02.dat");
				break;
			case SDLK_3:
				pal = fw().data->loadPalette("xcom3/ufodata/pal_03.dat");
				break;
			case SDLK_F6:
			{
				LogWarning("Writing voxel view to tileviewvoxels.png");
				auto imageOffset = -this->getScreenOffset();
				auto img = std::dynamic_pointer_cast<RGBImage>(
				    this->map.dumpVoxelView({imageOffset, imageOffset + dpySize}, *this,
				                            mode == Mode::Battle ? currentZLevel : 10.0f));
				fw().data->writeImage("tileviewvoxels.png", img);
			}
		}
	}
	else if (e->type() == EVENT_KEY_UP)
	{
		switch (e->keyboard().KeyCode)
		{
			case SDLK_UP:
				scrollUp = false;
				break;
			case SDLK_DOWN:
				scrollDown = false;
				break;
			case SDLK_LEFT:
				scrollLeft = false;
				break;
			case SDLK_RIGHT:
				scrollRight = false;
				break;
		}
	}
	else if (e->type() == EVENT_MOUSE_MOVE)
	{
		Vec2<float> screenOffset = {this->getScreenOffset().x, this->getScreenOffset().y};
		// Offset by 4 since ingame 4 is the typical height of the ground, and game displays cursor
		// on top of the ground
		setSelectedTilePosition(this->screenToTileCoords(
		    Vec2<float>((float)e->mouse().X, (float)e->mouse().Y + 4 - 20) - screenOffset,
		    currentZLevel - 1));
	}
	else if (e->type() == EVENT_FINGER_MOVE)
	{
		// FIXME: Review this code for sanity
		if (e->finger().IsPrimary)
		{
			Vec3<float> deltaPos(e->finger().DeltaX, e->finger().DeltaY, 0);
			if (this->viewMode == TileViewMode::Isometric)
			{
				deltaPos.x /= isoTileSize.x;
				deltaPos.y /= isoTileSize.y;
				Vec3<float> isoDelta(deltaPos.x + deltaPos.y, deltaPos.y - deltaPos.x, 0);
				deltaPos = isoDelta;
			}
			else
			{
				deltaPos.x /= stratTileSize.x;
				deltaPos.y /= stratTileSize.y;
			}
			Vec3<float> newPos = this->centerPos - deltaPos;
			this->setScreenCenterTile(newPos);
		}
	}
}

void TileView::render()
{
	TRACE_FN;
	Renderer &r = *fw().renderer;
	r.clear();
	r.setPalette(this->pal);

	Vec3<float> newPos = this->centerPos;
	if (this->viewMode == TileViewMode::Isometric)
	{
		if (scrollLeft)
		{
			newPos.x -= isoScrollSpeed.x;
			newPos.y += isoScrollSpeed.y;
		}
		if (scrollRight)
		{
			newPos.x += isoScrollSpeed.x;
			newPos.y -= isoScrollSpeed.y;
		}
		if (scrollUp)
		{
			newPos.y -= isoScrollSpeed.y;
			newPos.x -= isoScrollSpeed.x;
		}
		if (scrollDown)
		{
			newPos.y += isoScrollSpeed.y;
			newPos.x += isoScrollSpeed.x;
		}
	}
	else if (this->viewMode == TileViewMode::Strategy)
	{
		if (scrollLeft)
			newPos.x -= stratScrollSpeed.x;
		if (scrollRight)
			newPos.x += stratScrollSpeed.x;
		if (scrollUp)
			newPos.y -= stratScrollSpeed.y;
		if (scrollDown)
			newPos.y += stratScrollSpeed.y;
	}
	else
	{
		LogError("Unknown view mode");
	}

	this->setScreenCenterTile(newPos);

	// screenOffset.x/screenOffset.y is the 'amount added to the tile coords' - so we want
	// the inverse to tell which tiles are at the screen bounds
	auto topLeft = offsetScreenToTileCoords(Vec2<int>{-isoTileSize.x, -isoTileSize.y}, 0);
	auto topRight = offsetScreenToTileCoords(Vec2<int>{dpySize.x, -isoTileSize.y}, 0);
	auto bottomLeft = offsetScreenToTileCoords(Vec2<int>{-isoTileSize.x, dpySize.y}, map.size.z);
	auto bottomRight = offsetScreenToTileCoords(Vec2<int>{dpySize.x, dpySize.y}, map.size.z);

	int minX = std::max(0, topLeft.x);
	int maxX = std::min(map.size.x, bottomRight.x);

	int minY = std::max(0, topRight.y);
	int maxY = std::min(map.size.y, bottomLeft.y);

	int zFrom = 0;
	int zTo = maxZDraw;

	switch (layerDrawingMode)
	{
		case LayerDrawingMode::UpToCurrentLevel:
			zFrom = 0;
			zTo = currentZLevel;
			break;
		case LayerDrawingMode::AllLevels:
			zFrom = 0;
			zTo = maxZDraw;
			break;
		case LayerDrawingMode::OnlyCurrentLevel:
			zFrom = currentZLevel - 1;
			zTo = currentZLevel;
			break;
	}

	// FIXME: A different algorithm is required in order to properly display big units.
	/*
		1) Rendering must go in diagonal lines. Illustration:

		CURRENT		TARGET

		147			136
		258			258
		369			479

		2) Objects must be located in the bottom-most, right-most tile they intersect
		(already implemented)
		
		3) Object can either occupy 1, 2 or 3 tiles on the X axis (only X matters)
		
		- Tiny objects (items, projectiles) occupy 1 tile always
		- Small typical objects (walls, sceneries, small units) occupy 1 tile when static, 
																  2 when moving on X axis
		- Large objects (large units) occupy 2 tiles when static, 3 when moving on x axis

		How to determine this value is TBD.

		4) When rendering we must check 1 tile ahead for 2-tile object 
		and 1 tile ahead and further on x axis for 3-tile object. 

		If present we must draw 1 tile ahead for 2-tile object
		or 2 tiles ahead and one tile further on x-axis for 3 tile object
		then resume normal draw order without drawing already drawn tiles
		
		Illustration:

		SMALL MOVING	LARGE STATIC	LARGE MOVING		LEGEND

										xxxxx > xxxxx6.		x		= tile w/o  object drawn
		 xxxx > xxxx48	xxxx > xxxx48	x+++  > x+++59		+		= tile with object drawn
		 xxx  > xxx37	x++  > x++37	x++O  > x++28.		digit	= draw order
		 x+O  > x+16	x+O  > x+16		x+OO  > x+13.		o		= object yet to draw
		 x?   > x25		x?   > x25		x?	  > x47.		?		= current position

		So, if we encounter a 2-tile (on x axis) object in the next position (x-1, y+1)
		then we must first draw tile (x-1,y+1), and then draw our tile, 
		and then skip drawing next tile (as we have already drawn it!)

		If we encounter a 3-tile (on x axis) object in the position (x-1,y+2)
		then we must first draw (x-1,y+1), then (x-2,y+2), then (x-1,y+2), then draw our tile,
		and then skip drawing next two tiles (as we have already drawn it) and skip drawing
		the tile (x-1, y+2) on the next row

		This is done best by having a set of Vec3<int>'s, and "skip next X tiles" variable.
		When encountering a 2-tile object, we inrement "skip next X tiles" by 1.
		When encountering a 3-tile object, we increment "skip next X tiles" by 2,
		and we add (x-1, y+2) to the set. 
		When trying to draw a tile we first check the "skip next X tiles" variable,
		if > 0 we decrement and continue.
		Second, we check if our tile is in the set. If so, we remove from set and continue.
		Third, we draw normally
	*/
	
	// FIXME: A different drawing algorithm is required for battle's strategic view
	/*
		First, draw everything except units and items
		Then, draw items only on current z-level
		Then, draw agents, bottom to top, drawing hollow sprites for non-current levels
	*/

	for (int z = zFrom; z < zTo; z++)
	{
		int currentLevel = z - currentZLevel;
		// FIXME: Draw double selection bracket for big units?
		// Find out when to draw selection bracket parts (if ever)
		Tile *selTileOnCurLevel = nullptr;
		Vec3<int> selTilePosOnCurLevel;
		sp<TileObject> drawBackBeforeThis;
		sp<Image> selectionImageBack;
		sp<Image> selectionImageFront;
		if (mode == Mode::Battle && this->viewMode == TileViewMode::Isometric)
		{
			if (selectedTilePosition.z >= z &&
			selectedTilePosition.x >= minX && selectedTilePosition.x < maxX &&
			selectedTilePosition.y >= minY && selectedTilePosition.y < maxY)
			{
				selTilePosOnCurLevel = { selectedTilePosition.x, selectedTilePosition.y, z };
				selTileOnCurLevel = map.getTile(selTilePosOnCurLevel.x, selTilePosOnCurLevel.y,
					selTilePosOnCurLevel.z);

				// Find where to draw back selection bracket
				auto object_count = selTileOnCurLevel->drawnObjects[0].size();
				for (size_t obj_id = 0; obj_id < object_count; obj_id++)
				{
					auto &obj = selTileOnCurLevel->drawnObjects[0][obj_id];
					if (!drawBackBeforeThis && obj->getType() != TileObject::Type::Ground)
						drawBackBeforeThis = obj;
				}
				// Find what kind of selection bracket to draw (yellow or green)
				// Yellow if this tile intersects with a unit
				if (selectedTilePosition.z == z)
				{
					bool foundUnit = false;
					for (auto &tile : selTileOnCurLevel->intersectingObjects)
						if (tile->getType() == TileObject::Type::Unit)
							foundUnit = true;
					if (foundUnit)
					{
						selectionImageBack = selectedTileFilledImageBack;
						selectionImageFront = selectedTileFilledImageFront;
					}
					else
					{
						selectionImageBack = selectedTileEmptyImageBack;
						selectionImageFront = selectedTileEmptyImageFront;
					}
				}
				else
				{
					selectionImageBack = selectedTileBackgroundImageBack;
					selectionImageFront = selectedTileBackgroundImageFront;
				}
			}
		}

		for (int layer = 0; layer < map.getLayerCount(); layer++)
		{
			for (int y = minY; y < maxY; y++)
			{
				for (int x = minX; x < maxX; x++)
				{
					auto tile = map.getTile(x, y, z);
					auto object_count = tile->drawnObjects[layer].size();
					// I assume splitting it here will improve performance?
					if (tile == selTileOnCurLevel && layer == 0)
					{
						for (size_t obj_id = 0; obj_id < object_count; obj_id++)
						{
							auto &obj = tile->drawnObjects[layer][obj_id];
							// Back selection image is drawn
							// between ground image and everything else
							if (obj == drawBackBeforeThis)
								r.draw(selectionImageBack,
								       tileToOffsetScreenCoords(selTilePosOnCurLevel) -
								           selectedTileImageOffset);
							Vec2<float> pos = tileToOffsetScreenCoords(obj->getPosition());
							obj->draw(r, *this, pos, this->viewMode, currentLevel);
						}
						// When done with all objects, draw the front selection image
						// (and back selection image if we haven't yet)
						if (!drawBackBeforeThis)
							r.draw(selectionImageBack,
							       tileToOffsetScreenCoords(selTilePosOnCurLevel) -
							           selectedTileImageOffset);
						r.draw(selectionImageFront,
						       tileToOffsetScreenCoords(selTilePosOnCurLevel) -
						           selectedTileImageOffset);
					}
					else
					{
						auto tile = map.getTile(x, y, z);
						auto object_count = tile->drawnObjects[layer].size();
						for (size_t obj_id = 0; obj_id < object_count; obj_id++)
						{
							auto &obj = tile->drawnObjects[layer][obj_id];
							Vec2<float> pos = tileToOffsetScreenCoords(obj->getPosition());
							obj->draw(r, *this, pos, this->viewMode, currentLevel);
						}
					}
				}
			}
		}
	}

	if (this->viewMode == TileViewMode::Strategy)
	{
		Vec2<float> centerIsoScreenPos = this->tileToScreenCoords(
		    Vec3<float>{this->centerPos.x, this->centerPos.y, 0}, TileViewMode::Isometric);

		/* Draw the rectangle of where the isometric view would be */
		Vec2<float> topLeftIsoScreenPos = centerIsoScreenPos;
		topLeftIsoScreenPos.x -= dpySize.x / 2;
		topLeftIsoScreenPos.y -= dpySize.y / 2;

		Vec2<float> topRightIsoScreenPos = centerIsoScreenPos;
		topRightIsoScreenPos.x += dpySize.x / 2;
		topRightIsoScreenPos.y -= dpySize.y / 2;

		Vec2<float> bottomLeftIsoScreenPos = centerIsoScreenPos;
		bottomLeftIsoScreenPos.x -= dpySize.x / 2;
		bottomLeftIsoScreenPos.y += dpySize.y / 2;

		Vec2<float> bottomRightIsoScreenPos = centerIsoScreenPos;
		bottomRightIsoScreenPos.x += dpySize.x / 2;
		bottomRightIsoScreenPos.y += dpySize.y / 2;

		Vec3<float> topLeftIsoTilePos =
		    this->screenToTileCoords(topLeftIsoScreenPos, 0.0f, TileViewMode::Isometric);
		Vec3<float> topRightIsoTilePos =
		    this->screenToTileCoords(topRightIsoScreenPos, 0.0f, TileViewMode::Isometric);
		Vec3<float> bottomLeftIsoTilePos =
		    this->screenToTileCoords(bottomLeftIsoScreenPos, 0.0f, TileViewMode::Isometric);
		Vec3<float> bottomRightIsoTilePos =
		    this->screenToTileCoords(bottomRightIsoScreenPos, 0.0f, TileViewMode::Isometric);

		Vec2<float> topLeftRectPos = this->tileToOffsetScreenCoords(topLeftIsoTilePos);
		Vec2<float> topRightRectPos = this->tileToOffsetScreenCoords(topRightIsoTilePos);
		Vec2<float> bottomLeftRectPos = this->tileToOffsetScreenCoords(bottomLeftIsoTilePos);
		Vec2<float> bottomRightRectPos = this->tileToOffsetScreenCoords(bottomRightIsoTilePos);

		r.drawLine(topLeftRectPos, topRightRectPos, this->strategyViewBoxColour,
		           this->strategyViewBoxThickness);
		r.drawLine(topRightRectPos, bottomRightRectPos, this->strategyViewBoxColour,
		           this->strategyViewBoxThickness);

		r.drawLine(bottomRightRectPos, bottomLeftRectPos, this->strategyViewBoxColour,
		           this->strategyViewBoxThickness);
		r.drawLine(bottomLeftRectPos, topLeftRectPos, this->strategyViewBoxColour,
		           this->strategyViewBoxThickness);
	}
}

void TileView::setZLevel(int zLevel)
{
	currentZLevel = clamp(zLevel, 1, maxZDraw);
	setScreenCenterTile(Vec3<float>{centerPos.x, centerPos.y, currentZLevel - 1});
}

int TileView::getZLevel() { return currentZLevel; }

void TileView::setLayerDrawingMode(LayerDrawingMode mode)
{
	if (this->mode == Mode::Battle)
		layerDrawingMode = mode;
}

bool TileView::isTransition() { return false; }

void TileView::setViewMode(TileViewMode newMode) { this->viewMode = newMode; }

TileViewMode TileView::getViewMode() const { return this->viewMode; }

Vec2<int> TileView::getScreenOffset() const
{
	Vec2<float> screenOffset = this->tileToScreenCoords(this->centerPos);

	return Vec2<int>{dpySize.x / 2 - screenOffset.x, dpySize.y / 2 - screenOffset.y};
}

void TileView::setScreenCenterTile(Vec3<float> center)
{
	fw().soundBackend->setListenerPosition({center.x, center.y, map.size.z / 2});
	Vec3<float> clampedCenter;
	if (center.x < 0.0f)
		clampedCenter.x = 0.0f;
	else if (center.x > map.size.x)
		clampedCenter.x = map.size.x;
	else
		clampedCenter.x = center.x;
	if (center.y < 0.0f)
		clampedCenter.y = 0.0f;
	else if (center.y > map.size.y)
		clampedCenter.y = map.size.y;
	else
		clampedCenter.y = center.y;
	if (center.z < 0.0f)
		clampedCenter.z = 0.0f;
	else if (center.z > map.size.z)
		clampedCenter.z = map.size.z;
	else
		clampedCenter.z = center.z;

	this->centerPos = clampedCenter;
}

void TileView::setScreenCenterTile(Vec2<float> center)
{
	this->setScreenCenterTile(Vec3<float>{center.x, center.y, currentZLevel});
}

Vec3<int> TileView::getSelectedTilePosition() { return selectedTilePosition; }

void TileView::setSelectedTilePosition(Vec3<int> newPosition)
{
	selectedTilePosition = newPosition;
	if (selectedTilePosition.x < 0)
		selectedTilePosition.x = 0;
	if (selectedTilePosition.y < 0)
		selectedTilePosition.y = 0;
	if (selectedTilePosition.z < 0)
		selectedTilePosition.z = 0;
	if (selectedTilePosition.x >= map.size.x)
		selectedTilePosition.x = map.size.x - 1;
	if (selectedTilePosition.y >= map.size.y)
		selectedTilePosition.y = map.size.y - 1;
	if (selectedTilePosition.z >= map.size.z)
		selectedTilePosition.z = map.size.z - 1;
}

}; // namespace OpenApoc
