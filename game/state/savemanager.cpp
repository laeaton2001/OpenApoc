﻿#include "game/state/savemanager.h"
#include "dependencies/tinyxml2/tinyxml2.h"
#include "framework/framework.h"
#include "framework/fs.h"
#include <algorithm>
#include <boost/filesystem.hpp>
#include <iomanip>

// boost uuid for generating temporary identifier for new save
#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // conversion to string

namespace uuids = boost::uuids;
namespace fs = boost::filesystem;

namespace OpenApoc
{
const UString saveManifestName = "save_manifest";
const UString saveFileExtension = ".save";

SaveManager::SaveManager() : saveDirectory(fw().Settings->getString("Resource.SaveDataDir")) {}

UString SaveManager::createSavePath(const UString &name) const
{
	UString result;
	result += saveDirectory;
	result += "/";
	result += name;
	result += saveFileExtension;
	return result;
}

static std::map<SaveType, UString> saveTypeNames{
    {Manual, "New saved game"}, {Quick, "Quicksave"}, {Auto, "Autosave"}};

std::future<sp<GameState>> SaveManager::loadGame(const SaveMetadata &metadata) const
{
	return loadGame(metadata.getFile());
}

std::future<sp<GameState>> SaveManager::loadGame(const UString &savePath) const
{
	UString saveArchiveLocation = savePath;
	auto loadTask = fw().threadPool->enqueue([saveArchiveLocation]() -> sp<GameState> {
		auto state = mksp<GameState>();
		if (!state->loadGame(saveArchiveLocation))
		{
			LogError("Failed to load '%s'", saveArchiveLocation.c_str());
			return nullptr;
		}
		state->initState();
		return state;
	});

	return loadTask;
}

std::future<sp<GameState>> SaveManager::loadSpecialSave(const SaveType type) const
{
	if (type == Manual)
	{
		LogError("Cannot load automatic save for type %i", (int)type);
		return std::async(std::launch::deferred, []() -> sp<GameState> { return nullptr; });
	}

	UString saveName;

	try
	{
		saveName = saveTypeNames.at(type);
	}
	catch (std::out_of_range)
	{
		LogError("Cannot find name of save type %i", (int)type);
		return std::async(std::launch::deferred, []() -> sp<GameState> { return nullptr; });
	}

	return loadGame(createSavePath(saveName));
}

bool writeArchiveWithBackup(const sp<SerializationArchive> archive, const UString &path, bool pack)
{
	fs::path savePath = path.str();
	fs::path tempPath;
	bool shouldCleanup = false;
	try
	{
		if (!fs::exists(savePath))
		{
			return archive->write(path, pack);
		}
		else
		{
			// WARNING! Dragons live here! Specifically dragon named miniz who hates windows paths
			// (or paths not starting with dot)
			// therefore I'm doing gymnasitcs here to backup and still pass original path string to
			// archive write
			// that is really bad, because if user clicks exit, save will be renamed to some random
			// junk
			// however it will still function as regular save file, so maybe not that bad?
			fs::path saveDirectory = savePath.parent_path();
			bool haveNewName = false;
			for (int retries = 5; retries > 0; retries--)
			{
				tempPath = saveDirectory /
				           (boost::uuids::to_string(uuids::random_generator()()) + ".save");
				if (!fs::exists(tempPath))
				{
					haveNewName = true;
					break;
				}
			}

			if (!haveNewName)
			{
				LogError("Unable to create temporary file at \"%s\"", tempPath.string().c_str());
				return false;
			}

			fs::rename(savePath, tempPath);
			shouldCleanup = true;
			bool saveSuccess = archive->write(path, pack);
			shouldCleanup = false;

			if (saveSuccess)
			{
				fs::remove_all(tempPath);
				return true;
			}
			else
			{
				if (fs::exists(savePath))
				{
					fs::remove_all(savePath);
				}
				fs::rename(tempPath, savePath);
			}
		}
	}
	catch (fs::filesystem_error exception)
	{
		if (shouldCleanup)
		{
			if (fs::exists(savePath))
			{
				fs::remove_all(savePath);
			}
			fs::rename(tempPath, savePath);
		}

		LogError("Unable to save game: \"%s\"", exception.what());
	}

	return false;
}

bool SaveManager::newSaveGame(const UString &name, const sp<GameState> gameState) const
{
	bool pack = Strings::ToInteger(fw().Settings->getString("Resource.SaveSkipPacking")) == 0;

	UString path = createSavePath("save_" + name).str();
	if (fs::exists(path.str()))
	{
		bool foundFreePath = false;
		for (int retries = 5; retries > 0; retries--)
		{
			path = createSavePath("save_" + name + std::to_string(rand()));
			if (!fs::exists(path.str()))
			{
				foundFreePath = true;
				break;
			}
		}

		if (!foundFreePath)
		{
			LogError("Unable to generate filename for save %s", name.c_str());
			return false;
		}
	}

	SaveMetadata manifest(name, path, time(0), Manual, gameState);
	return saveGame(manifest, gameState);
}

bool SaveManager::overrideGame(const SaveMetadata &metadata, const sp<GameState> gameState) const
{
	SaveMetadata updatedMetadata(metadata, time(0), gameState);
	return saveGame(updatedMetadata, gameState);
}

bool SaveManager::saveGame(const SaveMetadata &metadata, const sp<GameState> gameState) const
{
	bool pack = Strings::ToInteger(fw().Settings->getString("Resource.SaveSkipPacking")) == 0;
	const UString path = metadata.getFile();
	TRACE_FN_ARGS1("path", path);
	auto archive = SerializationArchive::createArchive();
	if (gameState->serialize(archive) && metadata.serializeManifest(archive))
	{
		return writeArchiveWithBackup(archive, path, pack);
	}

	return false;
}

bool SaveManager::specialSaveGame(SaveType type, const sp<GameState> gameState) const
{
	if (type == Manual)
	{
		LogError("Cannot create automatic save for type %i", (int)type);
		return false;
	}

	UString saveName;

	try
	{
		saveName = saveTypeNames.at(type);
	}
	catch (std::out_of_range)
	{
		LogError("Cannot find name of save type %i", (int)type);
		return false;
	}

	SaveMetadata manifest(saveName, createSavePath(saveName), time(0), type, gameState);
	return saveGame(manifest, gameState);
}

std::vector<SaveMetadata> SaveManager::getSaveList() const
{
	fs::path saveDirectory = fw().Settings->getString("Resource.SaveDataDir").str();
	std::vector<SaveMetadata> saveList;
	try
	{
		fs::path currentPath = fs::current_path().string();
		if (!fs::exists(saveDirectory))
		{
			LogError("Save directory \"%s\" not found ", saveDirectory.c_str());
			return saveList;
		}

		for (auto i = fs::directory_iterator(currentPath / saveDirectory);
		     i != fs::directory_iterator(); i++)
		{
			if (i->path().extension().string() != saveFileExtension.str())
			{
				continue;
			}

			std::string saveFileName = i->path().filename().string();
			// miniz can't read paths not starting with dor or with windows slashes
			UString savePath = saveDirectory.string() + "/" + saveFileName;
			if (auto archive = SerializationArchive::readArchive(savePath))
			{
				SaveMetadata metadata;
				if (metadata.deserializeManifest(archive, savePath))
				{
					saveList.push_back(metadata);
				}
				else // accept saves with missing manifest if extension is correct
				{
					saveList.push_back(SaveMetadata("Unknown(Missing manifest)", savePath, 0,
					                                SaveType::Manual, nullptr));
				}
			}
		}
	}
	catch (fs::filesystem_error er)
	{
		LogError("Error while enumerating directory: \"%s\"", er.what());
	}

	sort(saveList.begin(), saveList.end(), [](const SaveMetadata &lhs, const SaveMetadata &rhs) {
		return lhs.getCreationDate() > rhs.getCreationDate();
	});

	return saveList;
}

bool SaveMetadata::deserializeManifest(const sp<SerializationArchive> archive,
                                       const UString &saveFileName)
{
	auto root = archive->getRoot("", saveManifestName);
	if (!root)
	{
		return false;
	}

	auto nameNode = root->getNodeOpt("name");
	if (!nameNode)
	{
		return false;
	}
	this->name = nameNode->getValue();

	auto difficultyNode = root->getNodeOpt("difficulty");
	if (difficultyNode)
	{
		this->difficulty = difficultyNode->getValue();
	}

	auto saveDateNode = root->getNodeOpt("save_date");
	if (saveDateNode)
	{
		std::istringstream stream(saveDateNode->getValue().str());
		time_t timestamp;
		stream >> timestamp;
		this->creationDate = timestamp;
	}

	auto gameTicksNode = root->getNodeOpt("game_ticks");
	if (gameTicksNode)
	{
		gameTicks = gameTicksNode->getValueUInt();
	}

	auto typeNode = root->getNodeOpt("type");
	if (typeNode)
	{
		this->type = (SaveType)Strings::ToInteger(typeNode->getValue());
	}
	else
	{
		this->type = Manual;
	}

	this->file = saveFileName;
	return true;
}

bool SaveMetadata::serializeManifest(const sp<SerializationArchive> archive) const
{
	auto root = archive->newRoot("", saveManifestName);
	if (!root)
	{
		return false;
	}
	auto nameNode = root->addNode("name");
	nameNode->setValue(this->getName());

	auto difficultyNode = root->addNode("difficulty");
	difficultyNode->setValue(this->getDifficulty());

	auto savedateNode = root->addNode("save_date");
	savedateNode->setValue(std::to_string(time(0)));

	auto gameTicksNode = root->addNode("game_ticks");
	gameTicksNode->setValue(std::to_string(getGameTicks()));

	if (this->type != Manual)
	{
		auto typeNode = root->addNode("type");
		typeNode->setValue(Strings::FromInteger(this->type));
	}

	return true;
}

const time_t SaveMetadata::getCreationDate() const { return creationDate; }

SaveMetadata::SaveMetadata(){};
SaveMetadata::~SaveMetadata(){};
SaveMetadata::SaveMetadata(UString name, UString file, time_t creationDate, SaveType type,
                           const sp<GameState> gameState)
    : name(name), file(file), type(type), creationDate(creationDate)
{
	if (gameState)
	{
		gameTicks = gameState->gameTime.getTicks();
		// this->difficulty = gameState->difficulty; ?
	}
}
SaveMetadata::SaveMetadata(const SaveMetadata &metdata, time_t creationDate,
                           const sp<GameState> gameState)
    : name(metdata.name), file(metdata.file), type(metdata.type), creationDate(creationDate)
{
	if (gameState)
	{
		gameTicks = gameState->gameTime.getTicks();
		// this->difficulty = gameState->difficulty; ?
	}
}
const UString &SaveMetadata::getName() const { return name; }
const UString &SaveMetadata::getFile() const { return file; }
const UString &SaveMetadata::getDifficulty() const { return difficulty; }
const SaveType &SaveMetadata::getType() const { return type; }
const unsigned int SaveMetadata::getGameTicks() const { return gameTicks; }
}