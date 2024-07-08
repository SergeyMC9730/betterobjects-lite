#include <Geode/Geode.hpp>
#include <nlohmann/json.hpp>

using namespace geode::prelude;

#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/GameObject.hpp>

class ListingExParams {
public:
	bool _custom = false;
	float _origScale = 1.f;

	ListingExParams() {}

	operator nlohmann::json() {
		nlohmann::json json;

		json["c"] = _custom;
		json["o"] = _origScale;

		return json;
	}
	operator std::string() {
		nlohmann::json json = *this;

		return json.dump();
	}

	ListingExParams(std::string &json_string) {
		nlohmann::json json = nlohmann::json::parse(json_string);

		_custom = json["c"].get<bool>();
		_origScale = json["o"].get<double>();
	}
};

class ListingObject {
protected:
#define RESTRICTED_UNIQUE_ID 0
	int _uniqueID = RESTRICTED_UNIQUE_ID;

	void setUniqueID() {
		while (_uniqueID == RESTRICTED_UNIQUE_ID) {
			_uniqueID = rand();
		}

		log::debug("ListingObject::setUniqueID() = {}", _uniqueID);
	}
#undef RESTRICTED_UNIQUE_ID
public:
	enum ListingObjectType {
		ObjectCollection,
		Folder
	};

	enum ListingObjectType _type = ObjectCollection;
	std::string _name = "";

	std::vector<ListingObject> _folderContainer = {};
	std::string _objectContainer = "";
	
	std::string _displayedName = "";

	bool _collectionSelected = false;
	
	bool _root = false;

	std::string getObjectDefinition() const {
		if (_type == ObjectCollection) return "Custom Object";
		if (_type == Folder) return "Folder";

		return "Unknown Listing Object";
	}

	ListingObject(const ListingObject &ref) {
		// log::debug("copy constructor called with id={} ({})", ref.getUniqueID(), ref.getObjectDefinition());

		_uniqueID = ref.getUniqueID();
		_type = ref._type;
		_name = ref._name;
		_folderContainer = ref._folderContainer;
		_displayedName = ref._displayedName;
		_collectionSelected = ref._collectionSelected;
		_objectContainer = ref._objectContainer;
		_root = ref._root;
	}

	ListingObject(enum ListingObjectType type) {
		setUniqueID();

		_type = type;
		_name = "Unnamed " + std::to_string(getUniqueID());
	}
	ListingObject() {
		setUniqueID();
	}

	operator nlohmann::json() {
		nlohmann::json json;

		json["type"] = (int)_type;
		json["name"] = _name;

		nlohmann::json folderContainer = nlohmann::json::array();

		for (ListingObject &entry : _folderContainer) {
			nlohmann::json obj = entry;

			folderContainer.push_back(obj);
		}

		json["folderContainer"] = folderContainer;
		json["objectContainer"] = _objectContainer;

		json["uid"] = getUniqueID();

		return json;
	}

	operator std::string() {
		nlohmann::json j = *this;
		return j.dump(4);
	}

	ListingObject(std::string &json_string) {
		nlohmann::json data = nlohmann::json::parse(json_string);

		if (data.contains("type") && data["type"].is_number()) {
			_type = (enum ListingObjectType)(data["type"].get<int>());
		}
		if (data.contains("name") && data["name"].is_string()) {
			_name = data["name"];
		}
		if (data.contains("objectContainer") && data["objectContainer"].is_string()) {
			_objectContainer = data["objectContainer"];
		}
		if (data.contains("folderContainer") && data["folderContainer"].is_array()) {
			for (nlohmann::json &it : data["folderContainer"]) {
				if (!it.is_object()) continue;

				std::string val = it.dump();
				_folderContainer.push_back(val);
			}
		}

		if (data.contains("uid") && data["uid"].is_number()) {
			_uniqueID = data["uid"].get<int>();
		} else {
			setUniqueID();
		}
	}

	int getUniqueID() const {
		return _uniqueID;
	}
};

std::vector<GameObject *> _createObjectsFromColors();

class ListingObjectInteractionPopup;

namespace PMGlobal {
	struct CollectionStructure {
		int uniqueID;
		CCPoint position;
	};

	ListingObject root = ListingObject::Folder;
	CCArray *selectedObjects = nullptr;
	GJBaseGameLayer *baseGameLayer = nullptr;
	std::string selectedObjectData = "";
	int selectedUniqueID = 0;
	bool triggerButtonActivation = false;
	bool triggerButtonDisactivation = false;
	int touchIndex = -502;
	gd::string _currentLevel;
	ListingObjectInteractionPopup *instance = nullptr;

	std::array<gd::string, 595> tempArray1;
	std::array<void *, 595> tempArray2;

	std::vector<struct CollectionStructure> currentStructures;

	bool structureExists(int uniqueID, CCPoint pos) {
		for (struct CollectionStructure &structure : currentStructures) {
			if (structure.uniqueID == uniqueID && structure.position.x == pos.x && structure.position.y == pos.y) {
				return true;
			}
		}

		return false;
	}
	// bool hasStructureOnPosition(CCPoint pos) {

	// }
	std::vector<struct CollectionStructure> getStructuresOnPosition(CCPoint pos) {
		std::vector<struct CollectionStructure> structures;

		for (struct CollectionStructure &structure : currentStructures) {
			if (structure.position.x == pos.x && structure.position.y == pos.y) {
				structures.push_back(structure);
			}
		}

		return structures;
	}

	void removeStructureFromList(struct CollectionStructure &collection) {
		std::vector<struct CollectionStructure> new_vec;

		for (struct CollectionStructure &structure : currentStructures) {
			if (structure.uniqueID == collection.uniqueID && structure.position.x == collection.position.x && structure.position.y == collection.position.y) {
				continue;
			}

			new_vec.push_back(structure);
		}

		currentStructures = new_vec;
	}

	void save() {
		std::string text = root;

		std::string path = Mod::get()->getSaveDir().generic_string();
		std::string filename = fmt::format("{}/root.json", path);

		std::ofstream o(filename);

		o << text;

		o.close();
	}
	void recover() {
		std::string path = Mod::get()->getSaveDir().generic_string();
		std::string filename = fmt::format("{}/root.json", path);

		if (!std::filesystem::exists(filename)) return;

		std::ifstream t(filename);
		std::string str;

		{
			std::stringstream buffer;
			buffer << t.rdbuf();

			str = buffer.str();
		}

		t.close();

		root = str;
	}

	void accessSelectedObjects() {
		if (PMGlobal::selectedObjects == nullptr) {
			PMGlobal::selectedObjects = CCArray::create();
			PMGlobal::selectedObjects->retain();
		}
	}

	void crearArrayWithoutCleanup(CCArray *array) {
		while (array->count() != 0) {
			array->removeObjectAtIndex(0, false);
		}
	}

	std::vector<std::string> splitString(const char *str, char d, unsigned int max_entries = 0) {
		std::vector<std::string> result;

		do {
			const char *begin = str;

			while(*str != d && *str) str++;

			std::string _ready = {begin, str};
			if (!_ready.empty()) {
				result.push_back(_ready);
			}

			if (max_entries > 0 && result.size() > max_entries) {
				break;
			}
		} while (0 != *str++);

		int sz = result.size();
		if (result[sz - 1].ends_with(fmt::format("{}", d))) {
			result[sz - 1].pop_back();
		}

		return result;
	}

	// std::map<std::string, std::string> parseDict(std::string &object_string, char delim = ',') {
	// 	std::vector<std::string> data = splitString(object_string.data(), delim);

	// 	std::map<std::string, std::string> object_map;

	// 	bool _key = true;

	// 	int key;
	// 	std::string value; 

	// 	for (std::string el : data) {
	// 		if (_key) {
	// 			key = el;
	// 		} else {
	// 			value = el;

	// 			object_map[key] = value;
	// 		}

	// 		_key = !_key;
	// 	}

	// 	return object_map;
	// }

	std::map<int, std::string> parseObjectData(std::string &object_string, char delim = ',') {
		std::vector<std::string> data = splitString(object_string.data(), delim);

		std::map<int, std::string> object_map;

		bool _key = true;

		int key;
		std::string value; 

		for (std::string el : data) {
			if (_key) {
				key = std::stoi(el);
			} else {
				value = el;

				object_map[key] = value;
			}

			_key = !_key;
		}

		return object_map;
	}

	CCPoint getPositionFromString(std::string &object_string) {
		std::map<int, std::string> object_map = parseObjectData(object_string);

		float x = std::stod(object_map[2]);
		float y = std::stod(object_map[3]);

		CCPoint p = {x, y};

		return p;
	}

	std::string buildKVString(std::map<int, std::string> &object_map) {
		std::string res;

		for (auto [k, v] : object_map) {
			res += std::to_string(k) + "," + v + ",";
		}

		if (res.length() >= 1) {
			res.pop_back();
		}

		return res;
	}

	std::string setPositionToString(std::string &object_string, CCPoint pos) {
		std::map<int, std::string> object_map = parseObjectData(object_string);

		object_map[2] = std::to_string(pos.x);
		object_map[3] = std::to_string(pos.y);

		return buildKVString(object_map);
	}

	GameObject *createGameObject(std::string &object_string) {
		std::map<int, std::string> object_map = parseObjectData(object_string);

		tempArray1.fill("");
		tempArray2.fill(nullptr);

		for (auto [k, v] : object_map) {
			tempArray1[k] = v;
			tempArray2[k] = baseGameLayer;
		}

		std::vector v1(tempArray1.begin(), tempArray1.end());
		std::vector v2(tempArray2.begin(), tempArray2.end());

		GameObject *new_object = GameObject::objectFromVector(
			v1, v2, baseGameLayer, false
		);

		return new_object;
	}

	GameObject *copyGameObject(GameObject *_obj) {
		if (!_obj) return nullptr;

		std::string object_string = _obj->getSaveString(baseGameLayer);

		return createGameObject(object_string);
	}

	std::vector<GameObject *> copyObjectsWithRelativePos(bool copyLevelColors = false) {
		std::vector<GameObject *> result = {};

		float min_x;
		float min_y;

		std::vector<float> x_vec;
		std::vector<float> y_vec;

		accessSelectedObjects();

		for (int i = 0; i < selectedObjects->count(); i++) {
			CCObject *base_object = PMGlobal::selectedObjects->objectAtIndex(i);
			GameObject *game_object = typeinfo_cast<GameObject *>(base_object);

			if (game_object == nullptr) continue;	

			GameObject *new_object = copyGameObject(game_object);
			new_object->retain();

			result.push_back(new_object);

			x_vec.push_back(game_object->getPositionX());
			y_vec.push_back(game_object->getPositionY());
		}

		if (result.size() == 0) return result;

		min_x = *(std::min_element(x_vec.begin(), x_vec.end()));
		min_y = *(std::min_element(y_vec.begin(), y_vec.end()));

		log::debug("min_x={}; min_y={}", min_x, min_y);

		for (GameObject *ref : result) {
			auto pos = ref->getPosition();
			
			pos.x -= min_x;
			pos.y -= min_y - 90.f;

			ref->setPosition(pos);
		}

		if (copyLevelColors) {
			auto color_vec = _createObjectsFromColors();		

			result.insert(result.end(), color_vec.begin(), color_vec.end());
		}

		return result;
	}
}

#include <functional>

class ColorObject {
private:
	std::string _hsvObject = "";

	_ccColor3B _color = {};
	_ccColor3B _color2 = {};
	
	bool _hueEnabled = false;
	bool _blending = false;
	bool _copyOpacity = false;
	bool _legacyHue = false;

	int _target = 0;
	int _copyTarget = 0;
	int _unk00 = 1;
	int _unk01 = 0;

	float _opacity = 1.f;

	int getHueEnabled() {
		if (_hueEnabled) return 1;

		return -1;
	}
	bool hueEnabled() {
		return _hueEnabled || !_hsvObject.empty();
	}
public:
	ColorObject() {}

	/**
	 * key 1 - r
	 * key 2 - g
	 * key 3 - b
	 * key 4 - hue enabled (-1 on disabled)
	 * key 5 - blending
	 * key 6 - color id
	 * key 7 - opacity
	 * key 8 - legacy hue enabled
	 * key 9 - copied color id
	 * key 10 - hue map (hue, sat, br, sat rel, br rel)
	 * key 11 - ? (its always 255)
	 * key 12 - ? (its always 255)
	 * key 13 - ? (its always 255)
	 * key 15 - ? (its always 1)
	 * key 17 - copy opacity
	 * key 18 - ? (its always 0) 
	 */
	ColorObject(std::string &v) {
		log::debug("ColorObject: v = {}", v);

		std::map<int, std::string> kv_array = PMGlobal::parseObjectData(v, '_');

		if (kv_array.count(1)) _color.r = std::stoi(kv_array[1]);
		if (kv_array.count(2)) _color.g = std::stoi(kv_array[2]);
		if (kv_array.count(3)) _color.b = std::stoi(kv_array[3]);

		if (kv_array.count(4)) {
			int _v = std::stoi(kv_array[4]);

			if (_v == -1) {
				_hueEnabled = false;
			} else {
				_hueEnabled = true;
			}
		} else {
			_hueEnabled = true;
		}

		if (kv_array.count(5)) _blending = std::stoi(kv_array[5]);
		if (kv_array.count(8)) _legacyHue = std::stoi(kv_array[8]);
		if (kv_array.count(17)) _copyOpacity = std::stoi(kv_array[17]);

		if (kv_array.count(6)) _target = std::stoi(kv_array[6]);
		if (kv_array.count(9)) _copyTarget = std::stoi(kv_array[9]);
		if (kv_array.count(15)) _unk00 = std::stoi(kv_array[15]);
		if (kv_array.count(18)) _unk01 = std::stoi(kv_array[18]);

		if (kv_array.count(7)) _opacity = std::stof(kv_array[7]);

		if (kv_array.count(10)) _hsvObject = kv_array[10];

		if (kv_array.count(11)) _color2.r = std::stoi(kv_array[11]);
		if (kv_array.count(12)) _color2.g = std::stoi(kv_array[12]);
		if (kv_array.count(13)) _color2.b = std::stoi(kv_array[13]);
	}
	ColorObject(const ColorObject &ref) {
		_hsvObject = ref._hsvObject;
		_color = ref._color;
		_color2 = ref._color2;
		_hueEnabled = ref._hueEnabled;
		_blending = ref._blending;
		_copyOpacity = ref._copyOpacity;
		_legacyHue = ref._legacyHue;
		_target = ref._target;
		_copyTarget = ref._copyTarget;
		_unk00 = ref._unk00;
		_unk01 = ref._unk01;
		_opacity = ref._opacity;
	}

	// operator std::string() {
	// 	return fmt::format("1_{}_2_{}_3_{}_4_{}_5_{}_6_{}_7_{}_8_{}_9_{}_10_{}_11_{}_12_{}_13_{}_15_{}_17_{}_18_{}",
	// 		_color.r, _color.g, _color.b, 
	// 		getHueEnabled(),
	// 		(int)_blending,
	// 		_target,
	// 		_opacity,
	// 		(int)_legacyHue,
	// 		_copyTarget,
	// 		_hsvObject,
	// 		_color2.r, _color2.g, _color2.b,
	// 		_unk00,
	// 		(int)_copyOpacity,
	// 		_unk01
	// 	);
	// }

	void debug() {
		log::debug("ColorObject::debug: hue enabled: {}; hsv: {}; id={};", hueEnabled(), _hsvObject, _target);
	}

	std::string toTrigger(CCPoint pos) {
		std::string str = fmt::format("1,899,2,{},3,{},7,{},8,{},9,{},10,0.1,17,{},23,{},20,100,35,{}",
			pos.x, pos.y,
			_color.r, _color.g, _color.b,
			(int)_blending,
			_target,
			_opacity
		);

		if (_hueEnabled || !_hsvObject.empty()) {
			str += fmt::format(",49,{},41,{}",
				_hsvObject,
				(int)_hueEnabled
			);
		}

		if (_copyTarget != 0) {
			str += fmt::format(",50,{}",
				_copyTarget
			);
		}
		if (_copyOpacity) {
			str += ",60,1";
		}

		return str;
	}
};

class LevelStartObject {
private:
	std::vector<ColorObject> _colorObjects = {};
public:
	LevelStartObject(std::string &v) {
		std::vector<std::string> cv = {};

		{
			std::vector<std::string> _p = PMGlobal::splitString(v.c_str(), ',', 3);

			if (_p.size() < 2) return;

			cv = PMGlobal::splitString(_p[1].c_str(), '|');
		}

		for (std::string &ref : cv) {
			_colorObjects.push_back(ref);
		}
	}

	std::vector<ColorObject> &getColorObjects() {
		return _colorObjects;
	}
};

std::vector<GameObject *> _createObjectsFromColors() {
	std::vector<GameObject *> result;

	LevelEditorLayer *lel = typeinfo_cast<LevelEditorLayer *>(PMGlobal::baseGameLayer);
	PMGlobal::_currentLevel = lel->getLevelString();
	// log::debug("{}\n------------", PMGlobal::_currentLevel);

	LevelStartObject obj = PMGlobal::_currentLevel;
	auto vec = obj.getColorObjects();

	int offset = 0;

	for (ColorObject &col_ref : vec) {
		ColorObject copy(col_ref);
		copy.debug();
		
		std::string generatedTrigger = copy.toTrigger({-90.f, (float)offset});

		offset += 30;

		auto gameObject = PMGlobal::createGameObject(generatedTrigger);
		gameObject->retain();

		result.push_back(gameObject);
	}

	return result;
}


class ListingObjectInteractionPopup : public FLAlertLayer {
public:
	enum InteractionType {
		Name, Rename, ObjectCreate
	};
private:
	ListingObject *_object;
	enum InteractionType _interaction;
	TextInput *_input;
	CCLayer *_objectSelector = nullptr;
	CCMenuItemToggler *_toggler = nullptr;
	bool _copyLevelColors = true;

	std::function<void(ListingObjectInteractionPopup *)> _onComplete = nullptr;

	void initWithNameRename() {
		CCLayer *objectSelector = CCLayer::create();
		CCLayer *scale9layer = CCLayer::create();

		CCScale9Sprite *spr1 = CCScale9Sprite::create("GJ_square01.png");
		auto winsize = CCDirector::sharedDirector()->getWinSize();

		spr1->setContentSize({300, 125});
		
		scale9layer->addChild(spr1);
		objectSelector->addChild(scale9layer, 0);

		scale9layer->setPosition({winsize.width / 2, winsize.height / 2});

		std::string title;
		std::string textInputTitle;

		if (_interaction == Name || _interaction == ObjectCreate) {
			title = fmt::format("Set {} Name", _object->getObjectDefinition());
			textInputTitle = fmt::format("Enter the {} name...", _object->getObjectDefinition());
		} else if (_interaction == Rename) {
			title = fmt::format("Rename {}", _object->getObjectDefinition());
			textInputTitle = fmt::format("Enter the {} name...", _object->getObjectDefinition());
		}

		if (_interaction == ObjectCreate) {
			title = fmt::format("Create {}", _object->getObjectDefinition());
		}

		auto bmf = CCLabelBMFont::create(title.c_str(), "bigFont.fnt");
		bmf->setScale(0.65f);
		bmf->setPositionX(winsize.width / 2);
		bmf->setPositionY(winsize.height / 2 + spr1->getContentSize().height / 2 - 20.f);
				
		objectSelector->addChild(bmf, 1);

		auto exitBtn = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
		auto btn3 = CCMenuItemSpriteExtra::create(
			exitBtn, this, menu_selector(ListingObjectInteractionPopup::onExitButton)
		);

		CCMenu *men2 = CCMenu::create();
    
		men2->setPosition({
			winsize.width / 2 - spr1->getContentSize().width / 2,
			winsize.height / 2 + spr1->getContentSize().height / 2
		});
		men2->addChild(btn3);

		objectSelector->addChild(men2, 2);
		
		TextInput *in = TextInput::create(200, textInputTitle, "chatFont.fnt");
		in->setPosition(winsize.width / 2, winsize.height / 2);
		in->setAnchorPoint({0.5f, 0.5f});
		in->setCallback([this](const std::string &value) {
			_object->_name = value;
		});

		if (!_object->_name.empty()) {
			in->setString(_object->_name);
		}

		_input = in;

		objectSelector->addChild(in, 2);

		if (_interaction == ObjectCreate) {
			CCArray *container = CCArray::create();
			container->retain();

			_toggler = GameToolbox::createToggleButton(
				"Copy Level Colors",
				menu_selector(ListingObjectInteractionPopup::onToggle1),
				true,
				men2,
				{winsize.width / 2 - 50.f + 3.f, winsize.height / 2 - 35.f},
				objectSelector,
				objectSelector,
				0.7f,
				0.7f,
				100.f,
				{7.f, 0.f},
				"bigFont.fnt",
				false,
				1,
				container
			);
			
			if (_toggler == nullptr) {
				log::debug("Error while creating toggler using GameToolbox::createToggleButton: object is nullptr");
			} else {
				_toggler->setID("copy-level-colors");
			}
		}

		m_mainLayer->addChild(objectSelector);

		auto base = CCSprite::create("square.png");
		base->setPosition({ 0, 0 });
		base->setScale(500.f);
		base->setColor({0, 0, 0});
		base->setOpacity(0);
		base->runAction(CCFadeTo::create(0.3f, 125));

		this->addChild(base, -1);
	}
public:
	static ListingObjectInteractionPopup *create(ListingObject *object, enum InteractionType interaction) {
		ListingObjectInteractionPopup* pRet = new ListingObjectInteractionPopup(); 
		if (pRet && pRet->init(object, interaction)) { 
			pRet->autorelease();
			return pRet;
		} else {
			delete pRet;
			pRet = 0;
			return 0; 
		} 
	}

	ListingObject *getObject() {
		return _object;
	}

	void setCallback(std::function<void(ListingObjectInteractionPopup *)> onComplete) {
		_onComplete = onComplete;
	}

	void onToggle1(CCObject *sender) {
		CCMenuItemToggler *toggler = typeinfo_cast<CCMenuItemToggler *>(sender);

		if (toggler == nullptr) return;

		PMGlobal::instance->_copyLevelColors = !toggler->isToggled();

		log::debug("ListingObjectInterationPopup::onToggle1: {}", PMGlobal::instance->shouldCopyLevelColors());
	}

	void keyBackClicked() override {
		if (_object->_name.empty() && _interaction == Rename) {
			std::string desc = fmt::format("<cy>{} name</c> cannot be <cr>empty</c>!", _object->getObjectDefinition());

			FLAlertLayer::create("Error", desc, "OK")->show();

			return;
		}

		if (_onComplete != nullptr) {
			_onComplete(this);
		}

		FLAlertLayer::keyBackClicked();
	}

	void onExitButton(CCObject *sender) {
		keyBackClicked();
	}

	bool init(ListingObject *object, enum InteractionType interaction) {
		if (!FLAlertLayer::init(0)) return false;
		
		_object = object;
		_interaction = interaction;

		PMGlobal::instance = this;
		
		if (interaction == Name || interaction == Rename || interaction == ObjectCreate) {
			initWithNameRename();
		}

    	show();

		return true;
	}

	void registerWithTouchDispatcher() override {
		CCTouchDispatcher *dispatcher = cocos2d::CCDirector::sharedDirector()->getTouchDispatcher();

    	dispatcher->addTargetedDelegate(this, PMGlobal::touchIndex + 64, true);;
	}

	bool shouldCopyLevelColors() {
		return _copyLevelColors;
	}
};

class CustomObjectListingPopup : public FLAlertLayer {
private:
	CCLayer *_objectSelector;
	CCScale9Sprite *_spr1;

	ListingObject _root;

	CCMenu *_folderItems;
	CCMenu *_actionItems;

	CustomObjectListingPopup *_parentPopup = nullptr;
	int _entryID = 0;

	bool _selectingItems = false;

	std::function<void(CustomObjectListingPopup *)> _onRootModify = nullptr;

	std::vector<int> _selectedEntries = {};

	bool _rootEntry = false;

	bool tNotRestrited(int v, std::vector<int> restricted) {
		for (int _v : restricted) {
			if (_v == v) return false;
		}

		return true;
	}

	bool entrySelected(int id) {
		return !tNotRestrited(id, _selectedEntries);
	}

	void callCallback() {
		if (_onRootModify != nullptr) {
			_onRootModify(this);
		}
	}

	enum ButtonsType {
		BFolderBasic,
		BSelectSingular,
		BSelectMutliple,
		BSelectZero,
		BFolderEmpty
	};

	void rebuildFolderListing() {
		_folderItems->removeAllChildrenWithCleanup(true);

		log::debug("rebuildFolderListing: rebuilding _folderItems");

		int entry_index = 0;

		for (ListingObject &folder_entry : _root._folderContainer) {	
			auto entry_btn = addGuiObject(folder_entry);

			entry_index++;
			if (entry_btn == nullptr) continue;

			entry_btn->setTag(entry_index - 1);
		}
	}

	bool rootHasUniqueID(int uid) {
		for (ListingObject &obj : _root._folderContainer) {
			if (obj.getUniqueID() == uid) return true;
		}

		return false;
	}

	CCNode *findItem(int id) {
		CCNode *ch = _folderItems->getChildByTag(id);

		if (!ch) return nullptr;

		return ch;
	}

	void selectItem(int id) {
		if (id < 0) {
			log::debug("selectItem: bad item id {}", id);

			return;
		}

		if (_selectingItems) {
			if (entrySelected(id)) {
				log::debug("selectItem: item {} is selected", id);

				return;
			}

			_selectedEntries.push_back(id);
		}

		if (!findItem(id)) {
			log::debug("selectItem: cound not find item {}", id);

			return;
		}

		CCSprite *spr = typeinfo_cast<CCSprite *>(findItem(id)->getChildByID("entry-sprite"));
		if (!spr) {
			log::debug("selectItem: could not find sprite inside item {}", id);

			return;
		}

		float scale = 1.f;

		spr->runAction(CCEaseExponentialOut::create(CCScaleTo::create(0.25, scale)));
		spr->runAction(CCTintTo::create(0.25f, 255, 255, 255));
	}
	void unselectItem(int id) {
		if (id < 0) {
			log::debug("unselectItem: bad item id {}", id);

			return;
		}

		if (_selectingItems) {
			if (!entrySelected(id)) {
				log::debug("unselectItem: item {} is not selected", id);

				return;
			}

			std::vector<int> new_v;
			for (int v : _selectedEntries) {
				if (v == id) continue;

				new_v.push_back(v);
			}

			_selectedEntries = new_v;
		}

		if (!findItem(id)) {
			log::debug("unselectItem: cound not find item {}", id);

			return;
		}

		CCSprite *spr = typeinfo_cast<CCSprite *>(findItem(id)->getChildByID("entry-sprite"));
		if (!spr) {
			log::debug("unselectItem: could not find sprite inside item {}", id);

			return;
		}

		float scale = 0.5f;

		spr->runAction(CCEaseExponentialOut::create(CCScaleTo::create(0.25, scale)));
		spr->runAction(CCTintTo::create(0.25f, 128, 128, 128));
	}

	void unselectItems() {
		CCArray *children = _folderItems->getChildren();

		if (children == nullptr) return;

		for (int i = 0; i < children->count(); i++) {
			CCObject *entry = children->objectAtIndex(i);
			int tag = -1;

			if (entry != nullptr) {
				tag = entry->getTag();
			}

			log::debug("i={}; tag={}", i, tag);

			unselectItem(tag);
		}
	}
	void selectItems() {
		CCArray *children = _folderItems->getChildren();

		for (int i = 0; i < children->count(); i++) {
			CCObject *entry = children->objectAtIndex(i);
			int tag = -1;

			if (entry != nullptr) {
				tag = entry->getTag();
			}

			log::debug("i={}; tag={}", i, tag);

			selectItem(tag);
		}
	}
public:
	// void keyBackClicked() override {


	// 	FLAlertLayer::keyBackClicked();
	// }

	void onObjectRename(CCObject *sender) {
		if (!findItem(_selectedEntries[0])) {
			log::debug("onObjectRename: _selectedEntries[0] does not exist");

			return;
		}

		ListingObject *refs = _root._folderContainer.data();

		ListingObject *_object = refs + _selectedEntries[0];
		ListingObject *object = new ListingObject(_object->_type);
		object->_name = _object->_name;

		std::string _oldName = object->_name;

		ListingObjectInteractionPopup *popup = ListingObjectInteractionPopup::create(object, ListingObjectInteractionPopup::Rename);
	
		popup->setCallback([this, _oldName, object, _object](ListingObjectInteractionPopup *popup) {
			log::debug("renaming done!");
			log::debug("new name: {}", object->_name);
			log::debug("old name: {}", _oldName);

			if (object->_name == _oldName) return;

			_object->_name = object->_name;

			delete object;

			for (ListingObject &folder_entry : _root._folderContainer) {
				if (folder_entry._name == object->_name) {
					std::string desc = fmt::format("<cy>{} name</c> should be <cp>unique</c>!", object->getObjectDefinition());

					FLAlertLayer::create("Error", desc, "OK")->show();
					
					object->_name = _oldName;

					return;
				}
			}

			this->updateRootRecursive();
			this->callCallback();

			this->rebuildFolderListing();

			// CustomObjectListingPopup *new_popup = CustomObjectListingPopup::create(_root);
			// new_popup->_parentPopup = this->_parentPopup;
			// new_popup->_entryID = this->_entryID;
			// new_popup->_onRootModify = this->_onRootModify;

			// this->keyBackClicked();
		});
	}

	void onDeleteItems(CCObject *sender) {
		for (int j = 0; j < 1; j++) {
			std::vector<int> uniques;

			auto selectedEntries = _selectedEntries;

			for (int i : selectedEntries) {
				unselectItem(i);

				int id = _root._folderContainer[i].getUniqueID();

				if (id == PMGlobal::selectedUniqueID) {
					PMGlobal::selectedUniqueID = 0;
					PMGlobal::selectedObjectData = "";
				}

				uniques.push_back(id);
			}

			for (int i : uniques) {
				log::debug("unique = {}", i);
			}
			for (int i : _selectedEntries) {
				log::debug("entry = {}", i);
			}

			_folderItems->updateLayout();

			std::vector<ListingObject> new_container;

			log::debug("onDeleteItems: _root._folderContainer");

			for (auto &entry : _root._folderContainer) {
				if (tNotRestrited(entry.getUniqueID(), uniques)) {
					new_container.push_back(entry);
				}
			}

			_root._folderContainer = new_container;

			updateRootRecursive();
			callCallback();

			rebuildFolderListing();

			_selectedEntries.clear();	
		}

		updateButtonsSelect();
	}

	void onCreateCustomObject(CCObject *sender) {
		if (PMGlobal::baseGameLayer == nullptr) return;

		PMGlobal::accessSelectedObjects();
		if (PMGlobal::selectedObjects->count() == 0) {
			FLAlertLayer::create("Error", "You have <cy>to select some objects</c> to create a <cp>collection</c> of them.", "OK")->show();

			return;
		}

		ListingObject *obj = new ListingObject(ListingObject::ObjectCollection);
		
		ListingObjectInteractionPopup *popup = ListingObjectInteractionPopup::create(obj, ListingObjectInteractionPopup::ObjectCreate);
		popup->setCallback([this](ListingObjectInteractionPopup *popup) {
			log::debug("done!");

			ListingObject *obj = popup->getObject();
			
			std::string serializedString = "";

			std::vector<GameObject *> object_vec = PMGlobal::copyObjectsWithRelativePos(popup->shouldCopyLevelColors());

			for (GameObject *game_object : object_vec) {
				serializedString += game_object->getSaveString(PMGlobal::baseGameLayer);
				serializedString += ";";
			}

			if (serializedString.empty()) {
				FLAlertLayer::create("Error", "Serialization process <cr>failed</c>: <cy>string is empty</c>.", "OK")->show();

				delete obj;

				return;
			}

			if (serializedString.length() >= 1) {
				serializedString.pop_back();
			}

			obj->_objectContainer = serializedString;

			this->addObject(*obj);
			delete obj;

			FLAlertLayer::create("Error", fmt::format("<cp>Object Collection</c> has been created out of <cy>{} objects</c>.", PMGlobal::selectedObjects->count()), "OK")->show();
		});
	}

	void exportEntries(std::vector<ListingObject> &entries) {
#ifdef _WIN32
		if (entries.size() == 0) return;

		OPENFILENAME ofn;
		char *filename = (char *)malloc(1024);

		ZeroMemory(filename, 1024);
		ZeroMemory(&ofn, sizeof(ofn));

		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFilter = "*.json\0";
		ofn.lpstrFile = filename;
		ofn.nMaxFile = 1024;
		ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		ofn.lpstrDefExt = "json";

		bool result = GetSaveFileName(&ofn);

		log::debug("GetSaveFileName={}", result);

		if (!result) return;
		
		std::string filename_str = filename;
		free(filename);

		log::debug("filename_str={}", filename_str);

		nlohmann::json json_array = nlohmann::json::array();

		for (ListingObject & obj : entries) {
			nlohmann::json jobj = obj;

			json_array.push_back(jobj);
		}

		std::string data = json_array.dump();

		std::ofstream out(filename_str);
		out << data;
		out.close();
#endif
	}

	void importEntries(std::vector<ListingObject> *entries, std::string path) {
		if (!std::filesystem::exists(path)) return;

		std::ifstream t(path);
		std::string str;

		{
			std::stringstream buffer;
			buffer << t.rdbuf();

			str = buffer.str();
		}

		t.close();

		nlohmann::json json_array = nlohmann::json::parse(str);
		if (!json_array.is_array()) return;

		for (nlohmann::json &ref : json_array) {
			std::string ser = ref.dump();

			ListingObject obj = ser;

			entries->push_back(obj);
		}
	}

	void onExport(CCObject *sender) {
		std::vector<ListingObject> objects;

		if (_selectingItems) {
			for (int id : _selectedEntries) {
				ListingObject &obj = _root._folderContainer[id];

				objects.push_back(obj);
			}
		} else {
			for (ListingObject &obj : _root._folderContainer) {
				objects.push_back(obj);
			}
		}

		exportEntries(objects);
	}
	void onImport(CCObject *sender) {
#ifdef _WIN32
		OPENFILENAME ofn;
		char *filename = (char *)malloc(1024);

		ZeroMemory(filename, 1024);
		ZeroMemory(&ofn, sizeof(ofn));

		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFilter = "*.json\0";
		ofn.lpstrFile = filename;
		ofn.nMaxFile = 1024;
		ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		ofn.lpstrDefExt = "json";

		bool result = GetOpenFileName(&ofn);

		log::debug("GetOpenFileName={}", result);

		if (!result) return;
		
		std::string filename_str = filename;
		free(filename);

		log::debug("filename_str={}", filename_str);

		std::vector<ListingObject> *objects = new std::vector<ListingObject>();

		importEntries(objects, filename_str);

		for (auto it = objects->begin(); it != objects->end(); ++it) {
			if (rootHasUniqueID(it->getUniqueID())) continue;

			_root._folderContainer.push_back(*it);
		}

		updateRootRecursive();
		callCallback();

		rebuildFolderListing();

		delete objects;
#endif
	}

	void setupButtons(enum ButtonsType type) {
		auto actions = _actionItems;

		if (type == BFolderBasic || type == BFolderEmpty) {
			if (type != BFolderEmpty) {
				{
					auto select_spr = ButtonSprite::create("Select");

					select_spr->setScale(0.5f);

					auto btn = CCMenuItemSpriteExtra::create(
						select_spr,
						this,
						menu_selector(CustomObjectListingPopup::onSelect)
					);

					actions->addChild(btn);
				}
			}

			{
				auto add_co_spr = ButtonSprite::create("Create Custom Object");

				add_co_spr->setScale(0.5f);

				auto btn = CCMenuItemSpriteExtra::create(
					add_co_spr,
					this,
					menu_selector(CustomObjectListingPopup::onCreateCustomObject)
				);

				actions->addChild(btn);
			}
#ifdef _WIN32
			{
				auto import_spr = ButtonSprite::create("Import");

				import_spr->setScale(0.5f);

				auto btn = CCMenuItemSpriteExtra::create(
					import_spr,
					this,
					menu_selector(CustomObjectListingPopup::onImport)
				);

				actions->addChild(btn);
			}
		}
#endif

		if (type == BSelectZero || type == BSelectSingular || type == BSelectMutliple) {
			{
				auto select_spr = ButtonSprite::create("Unselect");

				select_spr->setScale(0.5f);

				auto btn = CCMenuItemSpriteExtra::create(
					select_spr,
					this,
					menu_selector(CustomObjectListingPopup::onSelect)
				);

				actions->addChild(btn);
			}
		}

		if (type == BSelectSingular || type == BSelectMutliple) {
			{
				auto delete_spr = ButtonSprite::create("Delete");

				delete_spr->setScale(0.5f);

				auto btn = CCMenuItemSpriteExtra::create(
					delete_spr,
					this,
					menu_selector(CustomObjectListingPopup::onDeleteItems)
				);

				actions->addChild(btn);
			}
		}

		if (type == BSelectSingular) {
			{
				auto rename_spr = ButtonSprite::create("Rename");

				rename_spr->setScale(0.5f);

				auto btn = CCMenuItemSpriteExtra::create(
					rename_spr,
					this,
					menu_selector(CustomObjectListingPopup::onObjectRename)
				);

				actions->addChild(btn);
			}
		}
#ifdef _WIN32
		{
			auto share_spr = ButtonSprite::create("Export");

			share_spr->setScale(0.5f);

			auto btn = CCMenuItemSpriteExtra::create(
				share_spr,
				this,
				menu_selector(CustomObjectListingPopup::onExport)
			);

			actions->addChild(btn);
		}
#endif

		// {
		// 	auto close_all_spr = ButtonSprite::create("Close All");

		// 	close_all_spr->setScale(0.5f);

		// 	auto btn = CCMenuItemSpriteExtra::create(
		// 		close_all_spr,
		// 		this,
		// 		menu_selector(CustomObjectListingPopup::onExitAll)
		// 	);

		// 	actions->addChild(btn);
		// }

		actions->updateLayout();
	}

	void updateButtonsBase() {
		_actionItems->removeAllChildrenWithCleanup(true);

		if (_folderItems->getChildrenCount() != 0) {
			setupButtons(CustomObjectListingPopup::BFolderBasic);
		} else {
			setupButtons(CustomObjectListingPopup::BFolderEmpty);
		}
	}
	void updateButtonsSelect() {
		// if (_selectedEntries.size() == 0 && _selectingItems) {
		// 	_selectingItems = !_selectingItems;
		// 	return updateButtonsBase();
		// }

		_actionItems->removeAllChildrenWithCleanup(true);

		if (_selectedEntries.size() == 0) {
			setupButtons(CustomObjectListingPopup::BSelectZero);
		} else if (_selectedEntries.size() == 1) {
			setupButtons(CustomObjectListingPopup::BSelectSingular);
		} else {
			setupButtons(CustomObjectListingPopup::BSelectMutliple);
		}
	}

	void setModifyCallback(std::function<void(CustomObjectListingPopup *)> onRootModify) {
		_onRootModify = onRootModify;
	}

	ListingObject &getRoot() {
		return _root;
	}

	std::string getPathRecursive() {
		std::string path;

		if (_parentPopup != nullptr) {
			path += _parentPopup->getPathRecursive();
		}

		path += _root._name + "/";

		return path;
	}

	void onEntryClick(CCObject *sender) {
		int id = sender->getTag();

		if (_selectingItems) {
			if (entrySelected(id)) {
				unselectItem(id);
			} else {
				selectItem(id);
			}
			
			updateButtonsSelect();

			return;
		}

		ListingObject entry = _root._folderContainer[id];

		if (entry._type == entry.Folder) {
			std::string entry_name = entry._name;
			entry._displayedName = getPathRecursive() + entry_name + "/";

			auto popup = CustomObjectListingPopup::create(entry);
			popup->_parentPopup = this;
			popup->_entryID = id;
		} else {
			ListingObject *entry_ptr = _root._folderContainer.data() + id;

			if (entry_ptr->getUniqueID() == PMGlobal::selectedUniqueID) {
				entry_ptr->_collectionSelected = true;
			}

			entry_ptr->_collectionSelected = !entry_ptr->_collectionSelected;
			log::debug("collectionSelected={}; ID={}", entry_ptr->_collectionSelected, id);

			CCNode *_sender = typeinfo_cast<CCNode *>(sender);
			CCSprite *spr = nullptr;

			if (_sender) {
				spr = typeinfo_cast<CCSprite *>(_sender->getChildByID("entry-sprite"));
			}

			if (entry_ptr->_collectionSelected) {
				PMGlobal::selectedObjectData = entry_ptr->_objectContainer;
				PMGlobal::selectedUniqueID = entry_ptr->getUniqueID();

				// auto editorUI = EditorUI::get();
				// editorUI->m_deselectBtn->setEnabled(true);
				// editorUI->m_deselectBtn->setColor({255, 255, 255});
				// editorUI->m_deselectBtn->setOpacity(255);

				if (spr) {
					spr->setColor({128, 128, 128});
				}
			} else {
				PMGlobal::selectedObjectData = "";
				PMGlobal::selectedUniqueID = 0;
				// PMGlobal::triggerButtonDisactivation = true;

				// auto editorUI = EditorUI::get();
				// editorUI->m_deselectBtn->setEnabled(true);
				// editorUI->m_deselectBtn->setColor({166, 166, 166});
				// editorUI->m_deselectBtn->setOpacity(175);

				if (spr) {
					spr->setColor({255, 255, 255});
				}
			}
		}
	}

	void updateRootRecursive() {
		if (_parentPopup != nullptr) {
			ListingObject *parentRoot = &_parentPopup->_root;
			
			if (parentRoot->_type == parentRoot->Folder) {
				parentRoot->_folderContainer[_entryID] = _root;
			}

			_parentPopup->callCallback();
			_parentPopup->updateRootRecursive();
		}
	}

	CCMenuItemSpriteExtra *addGuiObject(ListingObject &object, bool animate = false) {
		CCSprite *spr = nullptr;
		bool with_custom = false;

		ListingExParams params;

		if (object._type == object.Folder) {
			spr = CCSprite::createWithSpriteFrameName("gj_folderBtn_001.png");
		}
		else if (object._type == object.ObjectCollection) {
			spr = CCSprite::createWithSpriteFrameName("square_01_001.png");
		}

		if (spr != nullptr) {
			std::string s = params;

			spr->setUserObject(CCString::create(s.c_str()));
		} else {
			return nullptr;
		}

		if (object._type == object.ObjectCollection && object._collectionSelected) {
			spr->setColor({128, 128, 128});
		}

		spr->setID("entry-sprite");

		auto entry_btn = CCMenuItemSpriteExtra::create(
			spr,
			this,
			menu_selector(CustomObjectListingPopup::onEntryClick)
		);

		RowLayout *entry_layout = RowLayout::create();
		entry_layout->setGrowCrossAxis(true);
		entry_layout->setGap(3.5f);
		entry_layout->setAutoScale(false);

		entry_btn->setLayout(entry_layout);

		auto bmf = CCLabelBMFont::create(object._name.c_str(), "bigFont.fnt");
		bmf->setScale(0.25f);

		auto bmf_csz = bmf->getContentSize();
		bmf_csz.width *= bmf->getScale();

		auto spr_csz = spr->getContentSize();
		spr_csz.width *= spr->getScale();

		float max_width = std::max(bmf_csz.width, spr_csz.width);

		CCSize new_csz;
		new_csz.width = max_width;
		new_csz.height = bmf_csz.height;

		entry_btn->addChild(bmf);
		entry_btn->setContentSize(new_csz);
		entry_btn->updateLayout();

		_folderItems->addChild(entry_btn);
		_folderItems->updateLayout();

		if (_selectingItems) {
			int id = entry_btn->getTag();

			if (!entrySelected(id)) {
				selectItem(id);
				unselectItem(id);
			}

			updateButtonsSelect();
		} else {
			updateButtonsBase();
		}

		return entry_btn;
	}

	void addObject(ListingObject object) {
		if (_root._type == _root.Folder) {
			if (object._name.empty()) return;

			for (ListingObject &folder_entry : _root._folderContainer) {
				if (folder_entry._name == object._name) {
					std::string desc = fmt::format("<cy>{} name</c> should be <cp>unique</c>!", object.getObjectDefinition());

					FLAlertLayer::create("Error", desc, "OK")->show();

					return;
				}
			}

			_root._folderContainer.push_back(object);
			callCallback();

			updateRootRecursive();

			auto btn = addGuiObject(object, true);

			if (!btn) return;

			btn->setTag(_root._folderContainer.size() - 1);
		}
	}

	void onSelect(CCObject *sender) {
		_actionItems->removeAllChildrenWithCleanup(true);

		_selectedEntries.clear();

		_selectingItems = !_selectingItems;
		if (_selectingItems) {
			setupButtons(CustomObjectListingPopup::BSelectZero);
			_selectingItems = false;
			unselectItems();
			_selectingItems = true;
		} else {
			setupButtons(CustomObjectListingPopup::BFolderBasic);
			selectItems();
		}

		_actionItems->updateLayout();
	}
	void onExitAll(CCObject *sender) {
		CustomObjectListingPopup *parentPopup = _parentPopup;

		while (parentPopup != nullptr) {
			auto its_parent = parentPopup->_parentPopup;

			parentPopup->keyBackClicked();

			parentPopup = its_parent;
		}

		keyBackClicked();
	}
private:
	void setupWithFolder(ListingObject &listing) {
		log::debug("CustomObjectListingPopup::setupWithFolder();");

		auto winsize = CCDirector::sharedDirector()->getWinSize();

		auto bmf2 = CCLabelBMFont::create(listing._displayedName.c_str(), "goldFont.fnt");

		bmf2->setScale(0.6f);
		bmf2->setPositionX(winsize.width / 2);
		bmf2->setPositionY(winsize.height / 2 - _spr1->getContentSize().height / 2 + 20);
		bmf2->limitLabelWidth(_spr1->getContentSize().width - 15.f, 0.6f, 0.1f);
				
		_objectSelector->addChild(bmf2, 20);

		CCMenu *items = CCMenu::create();
		CCMenu *actions = CCMenu::create();
		CCLayer *folder_layer = CCLayer::create();

		_folderItems = items;
		_actionItems = actions;

		ColumnLayout *clayout = ColumnLayout::create();
		
		folder_layer->setLayout(clayout);

		_objectSelector->addChild(folder_layer, 2);

		RowLayout *layout = RowLayout::create();
		layout->setGrowCrossAxis(true);
		layout->setGap(7.f);
		layout->setCrossAxisOverflow(false);

		items->setLayout(layout);

		auto csz = _spr1->getContentSize();
		csz.height /= 2.f;

		auto csz2 = _spr1->getContentSize();
		csz2.height /= 4.f;

		items->setContentSize(csz);
		actions->setContentSize(csz2);
		folder_layer->setContentSize(_spr1->getContentSize());

		folder_layer->addChild(actions, 2);
		folder_layer->addChild(items, 2);

		folder_layer->setContentSize(_spr1->getContentSize());
		folder_layer->setPosition(winsize / 2.f);

		items->setPositionX(items->getContentSize().width / 2.f);
		actions->setPositionX(actions->getContentSize().width / 2.f);

		RowLayout *actions_layout = RowLayout::create();
		actions_layout->setGrowCrossAxis(true);
		actions_layout->setAutoScale(false);
		actions_layout->setCrossAxisOverflow(false);

		actions->setLayout(actions_layout);

		int entry_index = 0;

		for (ListingObject &folder_entry : listing._folderContainer) {
			log::debug("{} vs {} : {}", PMGlobal::selectedUniqueID, folder_entry.getUniqueID(), PMGlobal::selectedUniqueID == folder_entry.getUniqueID());

			if (PMGlobal::selectedUniqueID == folder_entry.getUniqueID()) {
				folder_entry._collectionSelected = true;
			}

			auto entry_btn = addGuiObject(folder_entry);

			entry_index++;
			if (entry_btn == nullptr) continue;

			entry_btn->setTag(entry_index - 1);
		}

		items->updateLayout();
		actions->updateLayout();
		folder_layer->updateLayout();

		items->setContentSize(csz);
		actions->setContentSize(csz2);

		updateButtonsBase();

		actions->updateLayout();

		auto line1 = CCSprite::createWithSpriteFrameName("floorLine_001.png");
        
		line1->setPosition({
			folder_layer->getContentSize().width / 2.f, 
			items->getPositionY() - items->getContentSize().height / 2.f - 10
		});
		line1->setScaleX(0.55f);
		line1->setBlendFunc({GL_SRC_ALPHA, GL_ONE});

		auto line2 = CCSprite::createWithSpriteFrameName("groundSquareShadow_001.png");
		line2->setScaleX(0.7f);
		line2->setPosition({
			folder_layer->getContentSize().width / 2.f,
			line1->getPositionY() - (line1->getContentSize().height / 2.f) - 
				((line2->getContentSize().height * line2->getScaleX()) / 2.f) 
			+ 3.f
		});
		line2->setRotation(-90.f);

		float _scale = _spr1->getContentSize().width / line2->getContentSize().width;

		line2->setScaleY(_scale);

		folder_layer->addChild(line1, 1);
		folder_layer->addChild(line2, 0);
	}
public:
	static CustomObjectListingPopup *create(ListingObject &listing) {
		log::debug("CustomObjectListingPopup::create();");

		CustomObjectListingPopup* pRet = new CustomObjectListingPopup(); 
		if (pRet && pRet->init(listing)) { 
			pRet->autorelease();
			return pRet;
		} else {
			delete pRet;
			pRet = 0;
			return 0; 
		} 
	}

	void onExitButton(CCObject *sender) {
		keyBackClicked();
	}

	bool init(ListingObject &listing) {
		log::debug("CustomObjectListingPopup::init();");

		if (!FLAlertLayer::init(0)) return false;
		
		_rootEntry = listing._root;
		_root = listing;

		if (_root._displayedName.empty()) {
			_root._displayedName = _root._name;
		}

		CCLayer *objectSelector = CCLayer::create();
		CCLayer *scale9layer = CCLayer::create();

		_objectSelector = objectSelector;
		_objectSelector->setID("object-selector");

		CCScale9Sprite *spr1 = CCScale9Sprite::create("GJ_square02.png");
		auto winsize = CCDirector::sharedDirector()->getWinSize();

		_spr1 = spr1;

		spr1->setContentSize({250, 250});

		scale9layer->addChild(spr1);
		objectSelector->addChild(scale9layer, 0);

		scale9layer->setPosition({winsize.width / 2, winsize.height / 2});

		auto bmf = CCLabelBMFont::create("Custom Object Selector", "bigFont.fnt");
		bmf->setScale(0.45f);
		bmf->setPositionX(winsize.width / 2);
		bmf->setPositionY(winsize.height / 2 + spr1->getContentSize().height / 2 - 20);
				
		objectSelector->addChild(bmf, 1);

		auto exitBtn = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
		auto btn3 = CCMenuItemSpriteExtra::create(
			exitBtn, this, menu_selector(CustomObjectListingPopup::onExitButton)
		);
		CCMenu *men2 = CCMenu::create();
		CCMenu *men3 = CCMenu::create();

		float padding = 20.f;
    
		men2->setPosition({
			winsize.width / 2 - spr1->getContentSize().width / 2,
			winsize.height / 2 + spr1->getContentSize().height / 2
		});
		men3->setPosition({
			winsize.width / 2 + spr1->getContentSize().width / 2 - padding,
			winsize.height / 2 + spr1->getContentSize().height / 2 - padding
		});

		men2->addChild(btn3);

		objectSelector->addChild(men2, 2);
		objectSelector->addChild(men3, 2);

		CCMenu *men1 = CCMenu::create();

		if (listing._type == listing.Folder) {
			setupWithFolder(listing);
		}

		m_mainLayer->addChild(objectSelector);

		auto base = CCSprite::create("square.png");
		base->setPosition({ 0, 0 });
		base->setScale(500.f);
		base->setColor({0, 0, 0});
		base->setOpacity(0);
		base->runAction(CCFadeTo::create(0.3f, 125));

		this->addChild(base, -1);

    	show();

		return true;
	}

	void registerWithTouchDispatcher() override {
		CCTouchDispatcher *dispatcher = cocos2d::CCDirector::sharedDirector()->getTouchDispatcher();

    	dispatcher->addTargetedDelegate(this, PMGlobal::touchIndex, true);

		;
	}
};

class $modify(XLevelEditorLayer, LevelEditorLayer) {
	bool init(GJGameLevel *level, bool p1) {
		if (!LevelEditorLayer::init(level, p1)) {
			return false;
		}

		PMGlobal::baseGameLayer = this;
		PMGlobal::currentStructures.clear();
		
		PMGlobal::accessSelectedObjects();
		PMGlobal::crearArrayWithoutCleanup(PMGlobal::selectedObjects);

		EditorUI *eui = EditorUI::get();
		CCMenu *undo = typeinfo_cast<CCMenu *>(eui->getChildByID("settings-menu"));

		CCSprite *spr = CCSprite::createWithSpriteFrameName("square_01_001.png");
		spr->setScale(0.85f);

		auto myButton = CCMenuItemSpriteExtra::create(
			spr,
			this,
			menu_selector(XLevelEditorLayer::onMyButton)
		);

		undo->addChild(myButton);
		undo->updateLayout();

		return true;
	}

	void goThroughArray(CCArray *arr, std::string name) {
		log::debug("going through array {} ({} items)", name, arr->count());

		for (int i = 0; i < arr->count(); i++) {
			auto obj = arr->objectAtIndex(i);

			if (obj == nullptr) {
				log::debug(" - {} -> nullptr", i);

				continue;
			}

			log::debug(" - {} -> {}", i, obj);

			GameObject *gobj = typeinfo_cast<GameObject *>(obj);

			if (gobj != nullptr) {
				log::debug("   - save string: {}", gobj->getSaveString(this));
			}
		}
	}

	void onMyButton(CCObject*) {
		PMGlobal::recover();

		// PMGlobal::_currentLevel = getLevelString();
		// // log::debug("{}\n------------", PMGlobal::_currentLevel);

		// LevelStartObject obj = PMGlobal::_currentLevel;
		// auto vec = obj.getColorObjects();
		
		// for (ColorObject &ref : vec) {
		// 	log::debug("trigger -> {}", ref.toTrigger({0, 0}));
		// }

		PMGlobal::root._root = true;
		auto popup = CustomObjectListingPopup::create(PMGlobal::root);

		popup->setModifyCallback([](CustomObjectListingPopup *popup) {
			PMGlobal::root = popup->getRoot();
			PMGlobal::root._root = true; 
			PMGlobal::save();

			log::debug("modified!");
		});
		
		// auto objects = PMGlobal::copyObjectsWithRelativePos();

		// for (int i = 0; i < objects.size(); i++) {
		// 	log::debug(" - {} -> {}", i, objects[i]->getSaveString(this));
		// }
		// PMGlobal::accessSelectedObjects();
		// goThroughArray(PMGlobal::selectedObjects, "PMGlobal::selectedObjects");
	}
};

// class $modify(GameObject) {
// 	static GameObject *objectFromVector(std::vector<gd::string>& p0, std::vector<void *>& p1, GJBaseGameLayer *p2, bool p3) {
// 		log::debug("GameObject::objectFromVector(({}), ({}), {}, {})", p0.size(), p1.size(), p2, p3);

// 		return GameObject::objectFromVector(p0, p1, p2, p3);
// 	}
// };

class $modify(EditorUI) {
	void selectObject(GameObject *obj, bool p1) {
		// log::debug("hook! p1={}", p1);

		EditorUI::selectObject(obj, p1);

		PMGlobal::accessSelectedObjects();
		PMGlobal::crearArrayWithoutCleanup(PMGlobal::selectedObjects);

		if (m_selectedObjects->count() == 0) {
			PMGlobal::selectedObjects->addObject(obj);
		}
	}

	void selectObjects(CCArray *p0, bool p1) {
		// log::debug("hook! p0={}; p1={}", p0->count(), p1);

		EditorUI::selectObjects(p0, p1);
		
		PMGlobal::accessSelectedObjects();
		PMGlobal::crearArrayWithoutCleanup(PMGlobal::selectedObjects);

		PMGlobal::selectedObjects->addObjectsFromArray(m_selectedObjects);
	}

	void deselectAll() {
		EditorUI::deselectAll();

		PMGlobal::accessSelectedObjects();
		PMGlobal::crearArrayWithoutCleanup(PMGlobal::selectedObjects);

		// log::debug("EditorUI::deselectAll();");
	}

	void deselectObject(GameObject *p0) {
		EditorUI::deselectObject(p0);

		PMGlobal::accessSelectedObjects();

		if (PMGlobal::selectedObjects->containsObject(p0)) {
			PMGlobal::selectedObjects->removeObject(p0, false);
		}

		// auto position = p0->getRealPosition();
		// auto structures = PMGlobal::getStructuresOnPosition(position);

		// log::debug("found {} structures", structures.size());

		// if (structures.size() != 0) {
		// 	for (struct PMGlobal::CollectionStructure &structure : structures) {
		// 		PMGlobal::removeStructureFromList(structure);
		// 	}
		// }
		// log::debug("EditorUI::deselectObject({}); | Pos={}", p0, p0->getRealPosition());
	}

	void clickOnPosition(CCPoint p0) {
		// if (PMGlobal::selectedObjectData.empty()) {
		// 	return EditorUI::clickOnPosition(p0);
		// }
		EditorUI::clickOnPosition(p0);

		// log::debug("EditorUI::clickOnPosition({});\nselectedObjectData={}\nm_cameraTest={}\nm_clickAtPosition={}", p0, PMGlobal::selectedObjectData, m_cameraTest, m_clickAtPosition);

		if (PMGlobal::selectedObjectData.empty()) return;

		auto alignedPos = getGridSnappedPos(m_clickAtPosition);

		CCPoint base_offset = {alignedPos.x, alignedPos.y - 105.f - 15.f + 30.f};

		if (PMGlobal::structureExists(PMGlobal::selectedUniqueID, base_offset)) return;

		struct PMGlobal::CollectionStructure structure = {
			PMGlobal::selectedUniqueID,
			base_offset
		};
		
		PMGlobal::currentStructures.push_back(structure);

		LevelEditorLayer *layer = typeinfo_cast<LevelEditorLayer *>(PMGlobal::baseGameLayer);
		std::vector<std::string> earlyObjects = PMGlobal::splitString(PMGlobal::selectedObjectData.c_str(), ';');

		CCArray *objectArray = CCArray::create();
		objectArray->retain();

		for (std::string _earlyObject : earlyObjects) {
			CCPoint old_pos = PMGlobal::getPositionFromString(_earlyObject);

			old_pos.x += base_offset.x,
			old_pos.y += base_offset.y;

			std::string new_data = PMGlobal::setPositionToString(_earlyObject, old_pos);

			auto temp_array = layer->createObjectsFromString(new_data, false, false);
			objectArray->addObjectsFromArray(temp_array);
		}

		// UndoObject *undo = createUndoObject(UndoCommand::New, false);
		// undo->retain();
		// undo->m_objects = objectArray;

		// layer->m_undoObjects->addObject(undo);

		deselectAll();
		selectObjects(objectArray, false);

		PMGlobal::crearArrayWithoutCleanup(objectArray);

		objectArray->release();
	}

	// bool init(LevelEditorLayer *layer) {
	// 	if (!EditorUI::init(layer)) return false;

	// 	scheduleUpdate();

	// 	return true;
	// }

	// void update(float delta) {
	// 	EditorUI::update(delta);

	// 	if (PMGlobal::triggerButtonActivation) {
	// 		log::debug("PMGlobal::triggerButtonActivation");
			
	// 		if (PMGlobal::selectedUniqueID != 0 && !PMGlobal::selectedObjectData.empty()) {
	// 			m_deselectBtn->setEnabled(true);
	// 			m_deselectBtn->setColor({255, 255, 255});
	// 			m_deselectBtn->setOpacity(255);
	// 		}
	// 	}
	// 	if (PMGlobal::triggerButtonDisactivation) {
	// 		log::debug("PMGlobal::triggerButtonDisactivation");

	// 		m_deselectBtn->setEnabled(false);
	// 		m_deselectBtn->setColor({166, 166, 166});
	// 		m_deselectBtn->setOpacity(175);
	// 	}

	// 	PMGlobal::triggerButtonActivation = false;
	// 	PMGlobal::triggerButtonDisactivation = false;
	// }
};