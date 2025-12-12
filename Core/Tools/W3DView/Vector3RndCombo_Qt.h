#pragma once

#include <QComboBox>

class Vector3Randomizer;
class Vector3;

void Fill_Vector3_Rnd_Combo(QComboBox* combo);
Vector3Randomizer* Vector3_Rnd_From_Combo_Index(int index, float value1, float value2 = 0, float value3 = 0);
int Combo_Index_From_Vector3_Rnd(Vector3Randomizer* randomizer);



