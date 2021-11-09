// ==================================================================================
// = This file implements the File Input/Outpost used by MapViewState. I separated it
// = into its own file because the MapViewState.cpp file was starting to get a little
// = out of control.
// ==================================================================================

#include "MapViewState.h"

#include "Route.h"

#include "../Cache.h"
#include "../Constants/Strings.h"
#include "../IOHelper.h"
#include "../StructureCatalogue.h"
#include "../StructureManager.h"
#include "../Map/TileMap.h"
#include "../Map/MapView.h"
#include "../XmlSerializer.h"

#include <NAS2D/Utility.h>
#include <NAS2D/Filesystem.h>
#include <NAS2D/StringUtils.h>
#include <NAS2D/Xml/XmlDocument.h>
#include <NAS2D/Xml/XmlMemoryBuffer.h>
#include <NAS2D/ParserHelper.h>

#include <map>
#include <string>
#include <vector>
#include <stdexcept>


extern std::map<int, std::string> LEVEL_STRING_TABLE;


extern int ROBOT_ID_COUNTER; /// \fixme Kludge



/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/
static void loadResorucesFromXmlElement(NAS2D::Xml::XmlElement* element, StorableResources& resources)
{
	if (!element) { return; }

	resources = readResources(element);
}


static void readRccRobots(std::string robotIds, RobotCommand& robotCommand, RobotPool& pool)
{
	for (const auto& string : NAS2D::split(robotIds, ','))
	{
		const auto robotId = NAS2D::stringTo<int>(string);
		for (auto* robot : pool.robots())
		{
			if (robot->id() == robotId)
			{
				robotCommand.addRobot(robot);
				break;
			}
		}
	}
}



/*****************************************************************************
 * CLASS FUNCTIONS
 *****************************************************************************/

void MapViewState::save(const std::string& filePath)
{
	auto& renderer = NAS2D::Utility<NAS2D::Renderer>::get();
	renderer.drawBoxFilled(NAS2D::Rectangle{0, 0, renderer.size().x, renderer.size().y}, NAS2D::Color{0, 0, 0, 100});
	const auto imageSaving = &imageCache.load("sys/saving.png");
	renderer.drawImage(*imageSaving, renderer.center() - imageSaving->size() / 2);
	renderer.update();

	NAS2D::Xml::XmlDocument doc;

	auto* root = NAS2D::dictionaryToAttributes(
		constants::SaveGameRootNode,
		{{{"version", constants::SaveGameVersion}}}
	);
	doc.linkEndChild(root);

	root->linkEndChild(serializeProperties());
	mTileMap->serialize(root);
	mMapView->serialize(root);
	root->linkEndChild(NAS2D::Utility<StructureManager>::get().serialize());
	root->linkEndChild(writeRobots(mRobotPool, mRobotList));
	root->linkEndChild(writeResources(mResourceBreakdownPanel.previousResources(), "prev_resources"));

	root->linkEndChild(NAS2D::dictionaryToAttributes("turns", {{{"count", mTurnCount}}}));

	const auto population = mPopulation.getPopulations();
	root->linkEndChild(NAS2D::dictionaryToAttributes(
		"population",
		{{
			{"morale", mCurrentMorale},
			{"prev_morale", mPreviousMorale},
			{"colonist_landers", mLandersColonist},
			{"cargo_landers", mLandersCargo},
			{"children", population.child},
			{"students", population.student},
			{"workers", population.worker},
			{"scientists", population.scientist},
			{"retired", population.retiree},
			{"mean_crime", mPopulationPanel.crimeRate()},
		}}
	));

	auto moraleChangeReasons = new NAS2D::Xml::XmlElement("morale_change");
	auto& moraleChangeList = mPopulationPanel.moraleReasonList();
	for (auto& [message, value] : moraleChangeList)
	{
		moraleChangeReasons->linkEndChild(NAS2D::dictionaryToAttributes(
			"change", {{{"message", message}, {"val", value}}}
		));
	}
	root->linkEndChild(moraleChangeReasons);

	// Write out the XML file.
	NAS2D::Xml::XmlMemoryBuffer buff;
	doc.accept(&buff);

	NAS2D::Utility<NAS2D::Filesystem>::get().write(filePath, buff.buffer());
}


NAS2D::Xml::XmlElement* MapViewState::serializeProperties()
{
	return NAS2D::dictionaryToAttributes(
		"properties",
		{{
			{"sitemap", mPlanetAttributes.mapImagePath},
			{"tset", mPlanetAttributes.tilesetPath},
			{"diggingdepth", mPlanetAttributes.maxDepth},
			{"meansolardistance", mPlanetAttributes.meanSolarDistance},
			{"difficulty", difficultyString(difficulty())},
		}}
	);
}


void MapViewState::load(const std::string& filePath)
{
	mPlanetAttributes = Planet::Attributes();
	resetUi();

	auto& renderer = NAS2D::Utility<NAS2D::Renderer>::get();
	renderer.drawBoxFilled(NAS2D::Rectangle{0, 0, renderer.size().x, renderer.size().y}, NAS2D::Color{0, 0, 0, 100});
	const auto imageLoading = &imageCache.load("sys/loading.png");
	renderer.drawImage(*imageLoading, renderer.center() - imageLoading->size() / 2);
	renderer.update();

	mBtnToggleConnectedness.toggle(false);
	mBtnToggleHeightmap.toggle(false);
	mPopulationPanel.clearMoraleReasons();

	if (!NAS2D::Utility<NAS2D::Filesystem>::get().exists(filePath))
	{
		throw std::runtime_error("File '" + filePath + "' was not found.");
	}

	scrubRobotList();
	NAS2D::Utility<StructureManager>::get().dropAllStructures();
	ccLocation() = CcNotPlaced;

	delete mTileMap;
	mTileMap = nullptr;

	auto xmlDocument = openSavegame(filePath);
	auto* root = xmlDocument.firstChildElement(constants::SaveGameRootNode);

	NAS2D::Xml::XmlElement* map = root->firstChildElement("properties");
	const auto dictionary = NAS2D::attributesToDictionary(*map);

	mPlanetAttributes.maxDepth = dictionary.get<int>("diggingdepth");
	mPlanetAttributes.mapImagePath = dictionary.get("sitemap");
	mPlanetAttributes.tilesetPath = dictionary.get("tset");
	mPlanetAttributes.meanSolarDistance = dictionary.get<float>("meansolardistance");

	difficulty(stringToEnum(difficultyTable, dictionary.get("difficulty", std::string{"Medium"})));

	StructureCatalogue::init(mPlanetAttributes.meanSolarDistance);
	mTileMap = new TileMap(mPlanetAttributes.mapImagePath, mPlanetAttributes.tilesetPath, mPlanetAttributes.maxDepth, 0, Planet::Hostility::None, false);
	mTileMap->deserialize(root);
	mMapView = std::make_unique<MapView>(*mTileMap);
	mMapView->deserialize(root);
	mMiniMap = std::make_unique<MiniMap>(*mMapView, mTileMap, mRobotList, mPlanetAttributes.mapImagePath);
	mDetailMap = std::make_unique<DetailMap>(*mMapView, *mTileMap, mPlanetAttributes.tilesetPath);
	mNavControl = std::make_unique<NavControl>(*mMapView, *mTileMap);

	delete mPathSolver;
	mPathSolver = new micropather::MicroPather(mTileMap);
	auto& routeTable = NAS2D::Utility<std::map<class MineFacility*, Route>>::get();
	routeTable.clear();

	/**
	 * In the case of loading a game, the Robot Command Center depends on the robot list
	 * having already been loaded in order to match up the robots in the save game to
	 * the RCC.
	 */
	readRobots(root->firstChildElement("robots"));
	readStructures(root->firstChildElement("structures"));

	mResourceBreakdownPanel.previousResources() = readResources(root->firstChildElement("prev_resources"));
	readPopulation(root->firstChildElement("population"));
	readTurns(root->firstChildElement("turns"));

	readMoraleChanges(root->firstChildElement("morale_change"));

	checkConnectedness();

	NAS2D::Utility<StructureManager>::get().updateEnergyProduction();
	NAS2D::Utility<StructureManager>::get().updateEnergyConsumed();
	NAS2D::Utility<StructureManager>::get().assignColonistsToResidences(mPopulationPool);

	updateRobotControl(mRobotPool);
	updateResidentialCapacity();
	updateStructuresAvailability();

	updateRoads();
	findMineRoutes();
	updateFood();
	updatePlayerResources();

	if (mTurnCount == 0)
	{
		if (NAS2D::Utility<StructureManager>::get().count() == 0)
		{
			mBtnTurns.enabled(false);
			populateStructureMenu();
		}
		else
		{
			/**
			 * There should only ever be one structure if the turn count is 0, the
			 * SEED Lander which at this point should not have been deployed.
			 */
			const auto& list = NAS2D::Utility<StructureManager>::get().getStructures<SeedLander>();
			if (list.size() != 1) { throw std::runtime_error("MapViewState::load(): Turn counter at 0 but more than one structure in list."); }

			SeedLander* seedLander = list[0];
			if (!seedLander) { throw std::runtime_error("MapViewState::load(): Structure in list is not a SeedLander."); }

			seedLander->deploySignal().connect(this, &MapViewState::onDeploySeedLander);

			mStructures.clear();
			mConnections.clear();
			mBtnTurns.enabled(true);
		}
	}
	else
	{
		populateStructureMenu();
	}

	checkCommRangeOverlay();
	checkSurfacePoliceOverlay();

	mMapChangedSignal();
}


void MapViewState::readRobots(NAS2D::Xml::XmlElement* element)
{
	mRobotPool.clear();
	mRobotList.clear();
	mRobots.clear();

	ROBOT_ID_COUNTER = 0;
	for (NAS2D::Xml::XmlElement* robotElement = element->firstChildElement(); robotElement; robotElement = robotElement->nextSiblingElement())
	{
		const auto dictionary = NAS2D::attributesToDictionary(*robotElement);

		const auto id = dictionary.get<int>("id");
		const auto type = dictionary.get<int>("type");
		const auto age = dictionary.get<int>("age");
		const auto production_time = dictionary.get<int>("production");
		const auto x = dictionary.get<int>("x", 0);
		const auto y = dictionary.get<int>("y", 0);
		const auto depth = dictionary.get<int>("depth", 0);
		const auto direction = dictionary.get<int>("direction", 0);

		ROBOT_ID_COUNTER = std::max(ROBOT_ID_COUNTER, id);

		Robot* robot = nullptr;
		switch (static_cast<Robot::Type>(type))
		{
		case Robot::Type::Digger:
			robot = mRobotPool.addRobot(Robot::Type::Digger, id);
			robot->taskComplete().connect(this, &MapViewState::onDiggerTaskComplete);
			static_cast<Robodigger*>(robot)->direction(static_cast<Direction>(direction));
			break;

		case Robot::Type::Dozer:
			robot = mRobotPool.addRobot(Robot::Type::Dozer, id);
			robot->taskComplete().connect(this, &MapViewState::onDozerTaskComplete);
			break;

		case Robot::Type::Miner:
			robot = mRobotPool.addRobot(Robot::Type::Miner, id);
			robot->taskComplete().connect(this, &MapViewState::onMinerTaskComplete);
			break;

		default:
			throw std::runtime_error("Unknown robot type in savegame.");
		}

		// Could be done in the default handler in the above switch
		// but may be better here as an explicit statement.
		if (!robot) { continue; }

		robot->fuelCellAge(age);

		if (production_time > 0)
		{
			robot->startTask(production_time);
			mRobotPool.insertRobotIntoTable(mRobotList, robot, &mTileMap->getTile({{x, y}, depth}));
			mRobotList[robot]->index(TerrainType::Dozed);
		}

		if (depth > 0)
		{
			mRobotList[robot]->excavated(true);
		}
	}

	if (mRobotPool.robotAvailable(Robot::Type::Digger)) { checkRobotSelectionInterface(Robot::Type::Digger); }
	if (mRobotPool.robotAvailable(Robot::Type::Dozer)) { checkRobotSelectionInterface(Robot::Type::Dozer); }
	if (mRobotPool.robotAvailable(Robot::Type::Miner)) { checkRobotSelectionInterface(Robot::Type::Miner); }
}


void MapViewState::readStructures(NAS2D::Xml::XmlElement* element)
{
	for (NAS2D::Xml::XmlElement* structureElement = element->firstChildElement(); structureElement != nullptr; structureElement = structureElement->nextSiblingElement())
	{
		const auto dictionary = NAS2D::attributesToDictionary(*structureElement);

		const auto x = dictionary.get<int>("x");
		const auto y = dictionary.get<int>("y");
		const auto depth = dictionary.get<int>("depth");

		const auto type = dictionary.get<int>("type");
		const auto age = dictionary.get<int>("age");
		const auto state = dictionary.get<int>("state");
		const auto direction = dictionary.get<int>("direction");
		const auto forced_idle = dictionary.get<bool>("forced_idle");
		const auto disabled_reason = dictionary.get<int>("disabled_reason");
		const auto idle_reason = dictionary.get<int>("idle_reason");

		const auto crime_rate = dictionary.get<int>("crime_rate", 0);
		const auto integrity = dictionary.get<int>("integrity", 0);

		const auto production_completed = dictionary.get<int>("production_completed", 0);
		const auto production_type = dictionary.get<int>("production_type", 0);

		const auto pop0 = dictionary.get<int>("pop0");
		const auto pop1 = dictionary.get<int>("pop1");

		auto& tile = mTileMap->getTile({{x, y}, depth});
		tile.index(TerrainType::Dozed);
		tile.excavated(true);

		auto structureId = static_cast<StructureID>(type);
		if (structureId == StructureID::SID_TUBE)
		{
			ConnectorDir connectorDir = static_cast<ConnectorDir>(direction);
			insertTube(connectorDir, depth, &mTileMap->getTile({{x, y}, depth}));
			continue; // FIXME: ugly
		}

		auto& structure = *StructureCatalogue::get(structureId);

		if (structureId == StructureID::SID_COMMAND_CENTER)
		{
			ccLocation() = {x, y};
		}

		if (structureId == StructureID::SID_MINE_FACILITY)
		{
			auto* mine = mTileMap->getTile({{x, y}, 0}).mine();
			if (mine == nullptr)
			{
				throw std::runtime_error("Mine Facility is located on a Tile with no Mine.");
			}

			auto& mineFacility = *static_cast<MineFacility*>(&structure);
			mineFacility.mine(mine);
			mineFacility.maxDepth(mTileMap->maxDepth());
			mineFacility.extensionComplete().connect(this, &MapViewState::onMineFacilityExtend);

			auto trucks = structureElement->firstChildElement("trucks");
			if (trucks)
			{
				mineFacility.assignedTrucks(NAS2D::attributesToDictionary(*trucks).get<int>("assigned"));
			}

			auto extension = structureElement->firstChildElement("extension");
			if (extension)
			{
				mineFacility.digTimeRemaining(NAS2D::attributesToDictionary(*extension).get<int>("turns_remaining"));
			}
		}

		if (structureId == StructureID::SID_AIR_SHAFT && depth != 0)
		{
			static_cast<AirShaft*>(&structure)->ug(); // force underground state
		}

		if (structureId == StructureID::SID_SEED_LANDER)
		{
			static_cast<SeedLander*>(&structure)->position({x, y});
		}

		if (structureId == StructureID::SID_AGRIDOME ||
			structureId == StructureID::SID_COMMAND_CENTER)
		{
			auto& foodProduction = *static_cast<FoodProduction*>(&structure);

			auto foodStorage = structureElement->firstChildElement("food");
			if (foodStorage == nullptr)
			{
				throw std::runtime_error("MapViewState::readStructures(): FoodProduction structure saved without a food level node.");
			}

			foodProduction.foodLevel(NAS2D::attributesToDictionary(*foodStorage).get<int>("level"));
		}

		structure.age(age);
		structure.forced_state_change(static_cast<StructureState>(state), static_cast<DisabledReason>(disabled_reason), static_cast<IdleReason>(idle_reason));
		structure.connectorDirection(static_cast<ConnectorDir>(direction));
		structure.integrity(integrity);

		if (forced_idle != 0) { structure.forceIdle(forced_idle != 0); }

		loadResorucesFromXmlElement(structureElement->firstChildElement("production"), structure.production());
		loadResorucesFromXmlElement(structureElement->firstChildElement("storage"), structure.storage());

		if (structure.structureClass() == Structure::StructureClass::Residence)
		{
			auto waste = structureElement->firstChildElement("waste");
			if (waste)
			{
				auto& residence = *static_cast<Residence*>(&structure);
				const auto wasteDictionary = NAS2D::attributesToDictionary(*waste);
				residence.wasteAccumulated(wasteDictionary.get<int>("accumulated"));
				residence.wasteOverflow(wasteDictionary.get<int>("overflow"));
			}
		}

		if (structure.structureClass() == Structure::StructureClass::Maintenance)
		{
			auto personnel = structureElement->firstChildElement("personnel");
			if (personnel)
			{
				auto& maintenanceFacility = *static_cast<MaintenanceFacility*>(&structure);
				maintenanceFacility.personnel(NAS2D::attributesToDictionary(*personnel).get<int>("assigned", 0));
				maintenanceFacility.resources(mResourcesCount);
			}
		}

		if (structure.isWarehouse())
		{
			auto& warehouse = *static_cast<Warehouse*>(&structure);
			warehouse.products().deserialize(NAS2D::attributesToDictionary(
				*structureElement->firstChildElement("warehouse_products")
			));
		}

		if (structure.isFactory())
		{
			auto& factory = *static_cast<Factory*>(&structure);
			factory.productType(static_cast<ProductType>(production_type));
			factory.productionTurnsCompleted(production_completed);
			factory.resourcePool(&mResourcesCount);
			factory.productionComplete().connect(this, &MapViewState::onFactoryProductionComplete);
		}

		if (structure.isRobotCommand())
		{
			auto robotsElement = structureElement->firstChildElement("robots");
			if (robotsElement)
			{
				const auto robotIds = NAS2D::attributesToDictionary(*robotsElement).get("robots");
				auto& robotCommand = *static_cast<RobotCommand*>(&structure);
				readRccRobots(robotIds, robotCommand, mRobotPool);
			}
		}

		if (structure.hasCrime())
		{
			structure.crimeRate(crime_rate);
		}

		structure.populationAvailable() = {pop0, pop1};

		NAS2D::Utility<StructureManager>::get().addStructure(&structure, &tile);
	}
}


void MapViewState::readTurns(NAS2D::Xml::XmlElement* element)
{
	if (element)
	{
		mTurnCount = NAS2D::attributesToDictionary(*element).get<int>("count");

		if (mTurnCount > 0)
		{
			mBtnTurns.enabled(true);
			populateStructureMenu();
		}
	}
}


/**
 * Reads the population tag.
 */
void MapViewState::readPopulation(NAS2D::Xml::XmlElement* element)
{
	if (element)
	{
		mPopulation = {};

		const auto dictionary = NAS2D::attributesToDictionary(*element);

		mLandersColonist = dictionary.get<int>("colonist_landers");
		mLandersCargo = dictionary.get<int>("cargo_landers");

		mCurrentMorale = dictionary.get<int>("morale");
		mPreviousMorale = dictionary.get<int>("prev_morale");

		const auto meanCrimeRate = dictionary.get<int>("mean_crime", 0);

		const auto children = dictionary.get<int>("children");
		const auto students = dictionary.get<int>("students");
		const auto workers = dictionary.get<int>("workers");
		const auto scientists = dictionary.get<int>("scientists");
		const auto retired = dictionary.get<int>("retired");

		mPopulationPanel.morale(mCurrentMorale);
		mPopulationPanel.old_morale(mPreviousMorale);
		mPopulationPanel.crimeRate(meanCrimeRate);

		mPopulation.addPopulation({children, students, workers, scientists, retired});
	}
}


void MapViewState::readMoraleChanges(NAS2D::Xml::XmlElement* moraleChangeElement)
{
	if (!moraleChangeElement) { return; }

	for (auto messageElement = moraleChangeElement->firstChildElement(); messageElement; messageElement = messageElement->nextSiblingElement())
	{
		const auto dictionary = NAS2D::attributesToDictionary(*messageElement);

		const auto message = dictionary.get("message");
		const auto val = dictionary.get<int>("val");

		mPopulationPanel.addMoraleReason(message, val);
	}
}
