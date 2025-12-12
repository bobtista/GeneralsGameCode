#include "Vector3RndCombo_Qt.h"
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef _WIN32
#include "v3_rnd.h"
#else
#include "GameEngineStubs.h"
#endif
#include <QComboBox>

const char* const RANDOMIZER_NAMES[Vector3Randomizer::CLASSID_MAXKNOWN] =
{
	"Solid Box",
	"Solid Sphere",
	"Hollow Sphere",
	"Solid Cylinder",
};

void Fill_Vector3_Rnd_Combo(QComboBox* combo)
{
	if (!combo)
		return;
	
	combo->clear();
	
	for (int index = 0; index < Vector3Randomizer::CLASSID_MAXKNOWN; index++)
	{
		combo->addItem(QString::fromLocal8Bit(RANDOMIZER_NAMES[index]));
	}
}

Vector3Randomizer* Vector3_Rnd_From_Combo_Index(int index, float value1, float value2, float value3)
{
	Vector3Randomizer* randomizer = nullptr;
	
	switch (index)
	{
		case Vector3Randomizer::CLASSID_SOLIDBOX:
			randomizer = new Vector3SolidBoxRandomizer(Vector3(value1, value2, value3));
			break;
			
		case Vector3Randomizer::CLASSID_SOLIDSPHERE:
			randomizer = new Vector3SolidSphereRandomizer(value1);
			break;
			
		case Vector3Randomizer::CLASSID_HOLLOWSPHERE:
			randomizer = new Vector3HollowSphereRandomizer(value1);
			break;
			
		case Vector3Randomizer::CLASSID_SOLIDCYLINDER:
			randomizer = new Vector3SolidCylinderRandomizer(value1, value2);
			break;
	}
	
	return randomizer;
}

int Combo_Index_From_Vector3_Rnd(Vector3Randomizer* randomizer)
{
	int index = 0;
	if (randomizer != nullptr)
	{
		index = static_cast<int>(randomizer->Class_ID());
	}
	return index;
}

