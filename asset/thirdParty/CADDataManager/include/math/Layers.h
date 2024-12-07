#pragma once
#include "global/baseDef.h"

class Layers {
public:
	Layers() = default;

	int getMask();

	void set(int channel);

	void enable(int channel);

	void enalbeAll(); 

	void toggle(int channel);

	void disable(int channel);

	void disableAll();

	bool test(Layers layers);

private:
	int mMask{ 0 | 1 };
};