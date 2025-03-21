#pragma once
#include "parser/serialization_generated.h"
#include "json.hpp"


namespace cadDataManager {
	class RMaterial : public std::enable_shared_from_this<RMaterial>
	{
	public:
		using Ptr = std::shared_ptr<RMaterial>;
		static Ptr create() {
			return std::make_shared<RMaterial>();
		}

		RMaterial() {};
		~RMaterial() {};

		void setAppearance(int id, std::string param);

		int getID() { return rMaterialID; };
		std::string getDiffuse() { return rDiffuse; };
		std::string getSpecular() { return rSpecular; };
		float getShininess() { return rShininess; };
		std::string getDTName() { return rDiffuseTextureName; };
		std::string getEmissive() { return rEmissive; };
		float getEIntensity() { return rEmissiveIntensity; };
		float getOpacity() { return rOpacity; };
		float getMetalness() { return rMetalness; };
		float getRoughness() { return rRoughness; };
		float getTransmission() { return rTransmission; };

	private:
		int                   rMaterialID;
		std::string           rDiffuse{ "#CCCCCC" };
		std::string           rSpecular{ "#4B4B4B" };
		float                 rShininess{ 16 };
		std::string           rDiffuseTextureName{ "" };
		std::string           rEmissive{ "#000000" };
		float                 rEmissiveIntensity{ 1 };
		float                 rOpacity{ 1 };
		float                 rMetalness{ 0 };
		float                 rRoughness{ 0 };
		float                 rTransmission{ 0 };
	};
}
