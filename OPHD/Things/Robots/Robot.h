#pragma once

#include "../Thing.h"


class Robot: public Thing
{
public:
	enum class Type
	{
		Digger,
		Dozer,
		Miner,

		None
	};

	using TaskCallback = NAS2D::Signals::Signal<Robot*>;

public:
	Robot(const std::string&, const std::string&, Type);
	Robot(const std::string&, const std::string&, const std::string&, Type);

	void startTask(int turns);

	void fuelCellAge(int age) { mFuelCellAge = age; }
	int fuelCellAge() const { return mFuelCellAge; }
	int turnsToCompleteTask() const { return mTurnsToCompleteTask; }

	bool selfDestruct() const { return mSelfDestruct; }
	void seldDestruct(bool value) { mSelfDestruct = value; }

	bool idle() const { return turnsToCompleteTask() == 0; }

	Type type() const { return mType; }

	TaskCallback& taskComplete() { return mTaskCompleteCallback; }

	void id(int newId) { mId = newId; }
	int id() const { return mId; }

protected:
	void incrementFuelCellAge() { mFuelCellAge++; }
	void updateTask();

private:
	int mId = 0;
	int mFuelCellAge = 0;
	int mTurnsToCompleteTask = 0;

	bool mSelfDestruct = false;

	Type mType{ Type::None };

	TaskCallback mTaskCompleteCallback;
};
