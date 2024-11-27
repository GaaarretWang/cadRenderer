# pragma once

#include "document/document3dData.h"
#include "global/baseDef.h"

class DocumentManager {
public:
	Document3dData::Ptr mActiveDocument3dData;
	std::unordered_map<std::string, Document3dData::Ptr> mDocument3dDataMap{};

	DocumentManager();
	~DocumentManager();
	static DocumentManager& getDocumentManager() {
		static DocumentManager documentManager;
		return documentManager;
	}

public:

	Document3dData::Ptr buildDocument3dData(std::string docName);

	Document3dData::Ptr getDocument3dData(std::string docName);

	void removeDocumentData(std::string docName);

	void clearDocumentData();

	Document3dData::Ptr setActiveDocument3dData(Document3dData::Ptr activeDocument3dData);

	Document3dData::Ptr setActiveDocument3dData(std::string docName);

	Document3dData::Ptr getActiveDocument3dData();
};