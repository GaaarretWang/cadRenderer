#include <vector>
namespace cadDataManager {
	const int triangleMaxNumber = 100000;

	static const std::vector<int> initConstIndex1() {
		int indexNumber = triangleMaxNumber * 3;
		std::vector<int> InitIndex(indexNumber);
		for (int i = 0; i < indexNumber; ++i)
		{
			InitIndex[i] = i;
		}
		return InitIndex;
	}

	static const std::vector<int> initConstIndex2() {
		std::vector<int> InitIndex(triangleMaxNumber * 3);
		for (int i = 0; i < triangleMaxNumber; ++i)
		{
			if (i % 2 == 0) {
				InitIndex[i * 3] = i;
				InitIndex[i * 3 + 1] = i + 1;
				InitIndex[i * 3 + 2] = i + 2;
			}
			else {
				InitIndex[i * 3] = i + 1;
				InitIndex[i * 3 + 1] = i;
				InitIndex[i * 3 + 2] = i + 2;
			}
		}
		return InitIndex;
	}

	const std::vector<int> InitIndex1 = initConstIndex1();

	const std::vector<int> InitIndex2 = initConstIndex2();
}
