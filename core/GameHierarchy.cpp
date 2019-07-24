#include "GameHierarchy.h"
//
#include "GameCamera.h"
#include "GameRender.h"
#include "GameGizmo.h"
#include "Engine.h"
#include "inspector.h"
#include "Screen.h"
#include "KFile.h"
#include "KImgui.h"
#include "KLog.h"
#include "KNum.h"
#include "KPath.h"
#include "KStd.h"
#include "KQuat.h"
#include "KVideo.h"
#include <algorithm> // sort

#define NODE_TEXT_COLOR_NORMAL     ImColor(255, 255, 255)
#define NODE_TEXT_COLOR_DISABLED   ImColor( 80,  80,  80)
#define NODE_TEXT_COLOR_INVISIBLE  ImColor(160, 160, 160)
#define NODE_TEXT_COLOR_PAUSED     ImColor(160,  64,  64)
#define NODE_TEXT_COLOR_WARNING    ImColor(255,  64,  64)


namespace mo {

static TEXID g_IconTex = NULL;

static const int ICON_W = 16;
static const int ICON_H = 12;
static const int ICON_TEX_W = 16;
static const int ICON_TEX_H = 64;
static const int NUM_ICONS = 3;

static void _MakeIconTex() {
	const int ICON_BMP_W = ICON_W;
	const int ICON_BMP_H = ICON_H * NUM_ICONS;
	const char *dat[ICON_BMP_H] = {
		"................",
		"................",
		"................",
		"....woooooow....",
		".woo..woow..oow.",
		"o....woooow....o",
		"o....woooow....o",
		"o....woooow....o",
		".woo..woow..oow.",
		"....woooooow....",
		"................",
		"................",

		".....oooooo.....",
		".......ow.......",
		".....woooow.....",
		"....o......o....",
		"...o...ow...o...",
		"..w....ow....w..",
		"..o....ow....o..",
		"..o....ooooo.o..",
		"..w..........w..",
		"...o........o...",
		"....o......o....",
		".....woooow.....",

		".......ow.......",
		".......ow.......",
		"....o..ow..o....",
		"...o...ow...o...",
		"..w....ow....w..",
		"..o....ow....o..",
		"..o....ow....o..",
		"..w....ow....w..",
		"...o........o...",
		"....o......o....",
		".....woooow.....",
		"................",
	};
	KImage img(ICON_TEX_W, ICON_TEX_H, Color32());
	for (int y=0; y<ICON_BMP_H; y++) {
		Log_assert(strlen(dat[y]) == ICON_W);
		for (int x=0; x<ICON_BMP_W; x++) {
			if (dat[y][x] == 'o') {
				img.setPixelFast(x, y, Color32(0xFF, 0xFF, 0xFF, 0xFF));
			} else if (dat[y][x] == 'w') {
				img.setPixelFast(x, y, Color32(0xFF, 0xFF, 0xFF, 0xAA));
			} else {
				img.setPixelFast(x, y, Color32(0x00, 0x00, 0x00, 0x00));
			}
		}
	}
#ifdef _DEBUG
	std::string png = img.saveToMemory();
	FileOutput("~gui_icons.png").write(png);
#endif
	g_IconTex = Video::createTexture2D(ICON_TEX_W, ICON_TEX_H);
	Video::writeImageToTexture(g_IconTex, img);
}



#pragma region HierarchyInspector
HierarchyInspector::HierarchyInspector(HierarchySystem *tree, Inspector *ins) {
	Log_assert(tree);
	tree_ = tree;
	main_inspector_ = ins;
	highlight_entity_ = NULL;
	range_selection_start_ = NULL;
	range_selection_end_ = NULL;
	range_selection_required_ = false;
	enable_mini_ctrl_ = false;
	tmp_visible_tree_entities_.clear();
	selected_entities_.clear();
	range_selection_buf_.clear();
	tmp_entities_.clear();
}
void HierarchyInspector::onStart() {
#ifndef NO_IMGUI
	if (main_inspector_) {
		_MakeIconTex();
		SystemEditorParams *editorP = main_inspector_->getSystemEditorParams(tree_);
		Log_assert(editorP);
		editorP->sort_primary = -3;
	}
#endif // !NO_IMGUI
}
bool HierarchyInspector::isSelected(EID e) {
	return e && std::find(selected_entities_.begin(), selected_entities_.end(), e) != selected_entities_.end();
}
void HierarchyInspector::selectEntity(EID e) {
#ifndef NO_IMGUI
	if (e && !isSelected(e)) {
		selected_entities_.push_back(e);
		if (main_inspector_) {
			main_inspector_->unselectAllEntities();
			main_inspector_->selectEntity(e);
		}
	}
#endif // !NO_IMGUI
}
void HierarchyInspector::unselectEntity(EID e) {
	if (e && isSelected(e)) {
		auto it = std::find(selected_entities_.begin(), selected_entities_.end(), e);
		selected_entities_.erase(it);
		if (range_selection_start_ == e) {
			range_selection_start_ = NULL;
		}
	}
}
void HierarchyInspector::clearSelection(bool clear_range_start) {
	selected_entities_.clear();
	if (clear_range_start) {
		range_selection_start_ = NULL;
	}
}
void HierarchyInspector::onGuiEntity(EID e) {
#ifndef NO_IMGUI
	const HierarchySystem::Node *node = tree_->_getTreeNode(e);
	if (node == NULL) return;

	if (node->parent) {
		EID parent_e = node->parent->e;
		const Path &name = tree_->getName(parent_e);
		ImGui::Text("Parent: #%d (0x%X) '%s'", EID_toint(parent_e), EID_toint(parent_e), name.u8());
	} else {
		ImGui::Text("Parent: (NONE)");
	}

	int index, first, second;
	tree_->getTraversalIndex(node, &index, &first, &second);
	ImGui::Text("Traverse Index: %d", index);
	ImGui::Text("Traverse First: %d", first);
	ImGui::Text("Traverse Second: %d", second);
	ImGui::Text("Num children: %d", node->children.size());

	ImGui::Separator();

	Matrix4 wmat;
	tree_->getLocalToWorldMatrix(e, &wmat);
	Vec3 wpos = wmat.transformZero();

	ImGui::Text("Pos in world: %g, %g, %g", wpos.x, wpos.y, wpos.z);

	bool changed = false;

	Vec3 pos = tree_->getPosition(e);
	if (ImGui::DragFloat3("Pos(X-Y-Z)", (float*)&pos, 1.0f)) {
		tree_->setPosition(e, pos);
		changed = true;
	}
	
	Vec3 scale = tree_->getScale(e);
	if (ImGui::DragFloat3("Scale(X-Y-Z)", (float*)&scale)) {
		tree_->setScale(e, scale);
		changed = true;
	}
	
	Quat rot = tree_->getRotation(e);
	if (ImGui_DragQuaternion("Rot(X-Y-Z-Deg)", &rot)) {
		tree_->setRotation(e, rot);
		changed = true;
	}
	
	Vec3 pivot = tree_->getPivot(e);
	if (ImGui::DragFloat3("Pivot", (float*)&pivot)) {
		tree_->setPivot(e, pivot);
		changed = true;
	}

	bool indep = tree_->isIndependent(e);
	if (ImGui::Checkbox("Independent of Herarchy", &indep)) {
		tree_->setIndependent(e, indep);
		changed = true;
	}

	// ツリー構造を持っているため、著接変更されていないエンティティも座標更新が必要になる場合がある
	// どこか一か所でも編集されたら再計算させる。
	// デバッグポーズ中でも再計算できるよう、いまここで Dirty 設定する（onFrameUpdate はポーズ解除されないと呼ばれない）
	if (changed) tree_->dirtyAllNodes();
#endif // !NO_IMGUI
}
void HierarchyInspector::movePrev() {
	if (! selected_entities_.empty()) {
		EID e = selected_entities_[0];
		EID parent = tree_->getParent(e);
		if (parent) {
			int i = tree_->getChildIndex(parent, e);
			tree_->moveChild(parent, i, i-1);
		} else {
			int i = tree_->getRootIndex(e);
			tree_->moveRoot(i, i-1);
		}
	}
}
void HierarchyInspector::moveNext() {
	if (! selected_entities_.empty()) {
		EID e = selected_entities_[0];
		EID parent = tree_->getParent(e);
		if (parent) {
			int i = tree_->getChildIndex(parent, e);
			tree_->moveChild(parent, i, i+1);
		} else {
			int i = tree_->getRootIndex(e);
			tree_->moveRoot(i, i+1);
		}
	}
}
void HierarchyInspector::onGuiMain() {
#ifndef NO_IMGUI
	ImGui::Text("Entities: %d", tree_->numAllEntities());
	ImGui::Checkbox("Show checkboxes", &enable_mini_ctrl_);

	// IDを指定して選択
	{
		EID e = selected_entities_.empty() ? 0 : selected_entities_[0];
		int id_int = EID_toint(e);
		if (ImGui::InputInt("Sel", (int*)&id_int)) {
			e = EID_from(id_int);
			clearSelection(true);
			selectEntity(e);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Select an entity by ID number");
		}
	}

	// 削除ボタン
	if (1) {
		if (ImGui::Button("Del")) {
			for (auto it=selected_entities_.begin(); it!=selected_entities_.end(); ++it) {
				EID e = *it;
				if (e) tree_->removeTree(e);
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Delete selected entity and its tree");
		}
	}

	// 前に移動ボタン
	if (1) {
		ImGui::SameLine();
		if (ImGui::Button("Up")) {
			movePrev();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Move selected entity to previous sibling");
		}
	}
	// 後ろに移動ボタン
	if (1) {
		ImGui::SameLine();
		if (ImGui::Button("Down")) {
			moveNext();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Move selected entity to next sibling");
		}
	}

	// ツリー上での範囲選択のために、
	// GUI上での表示順を記録しておくためのリストを初期化する
	tmp_visible_tree_entities_.clear();
//	range_selection_start_ = NULL; これはクリアしないこと！！
	range_selection_end_ = NULL;
	highlight_entity_ = NULL;

	// ルート要素ごとに、そのツリーをGUIに追加していく
	int numRoots = tree_->getRootCount();
	for (int i=0; i<numRoots; i++) {
		EID e = tree_->getRoot(i);
		onGuiTree(e);
	}

	// 全要素を取得
	tmp_entities_.clear();
	tree_->getAllEntities(tmp_entities_);

	// 親がNULLなのにルートに登録されていないノードはないか？
	for (size_t i=0; i<tmp_entities_.size(); i++) {
		EID e = tmp_entities_[i];
		if (tree_->getParent(e) == NULL) {
			if (tree_->getRootIndex(e) < 0) {
				ImGui::TextColored(NODE_TEXT_COLOR_WARNING, "[Null parent but not root]");
				onGuiTree(e);
			}
		}
	}

	// この時点で、highlight_entity_ には GUI 上でマウスポインタ直下にあるエンティティ項目が入っている。
	// そのエンティティをゲーム画面内で強調表示させる。
	if (main_inspector_) {
		main_inspector_->setHighlightedEntity(highlight_entity_);
	}
	// ツリービューでの範囲選択処理。複数の親子にまたがっていてもよい
	if (range_selection_start_ && range_selection_end_) {
		clearSelection(false); // 範囲選択情報は削除しない
		// 範囲選択が決定された
		size_t i = 0;
		EID end = NULL;
		// 範囲選択の先頭を探す。範囲選択の先頭よりも終端の方がリスト手前にある可能性に注意
		for (; i<tmp_visible_tree_entities_.size(); i++) {
			EID e = tmp_visible_tree_entities_[i];
			if (e == range_selection_start_) { 
				end = range_selection_end_; 
				break;
			}
			if (e == range_selection_end_) {
				end = range_selection_start_;
				break;
			}
		}
		// 範囲選択の終端までを選択する
		for (; i<tmp_visible_tree_entities_.size(); i++) {
			EID e = tmp_visible_tree_entities_[i];
			selectEntity(e);
			if (e == end) break;
		}
	}
#endif // !NO_IMGUI
}
static bool GUI_PutTreeNodeIcon(int icon_index, bool val, const char *hint_u8) {
	Log_assert(0 <= icon_index && icon_index < NUM_ICONS);
#ifndef NO_IMGUI
	ImGui_SImageParams p;
	p.w = ICON_W;
	p.h = ICON_H;
	p.u0 = 0.0f;
	p.u1 = (float)ICON_W / ICON_TEX_W;
	p.v0 = (float)ICON_H*(icon_index)   / ICON_TEX_H;
	p.v1 = (float)ICON_H*(icon_index+1) / ICON_TEX_H;
	p.color = val ? Color::WHITE : Color(0.3f, 0.3f, 0.3f, 1.0f);
	p.keep_aspect = false;
	ImGui_Image(g_IconTex, &p);
	if (ImGui::IsItemClicked()) {
		return true;
	}
	if (hint_u8 && hint_u8[0]) {
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(hint_u8);
		}
	}
#endif
	return false;
}

// ツリーノードの色を決める。Enabled, Visible 属性による
static ImColor GUI_GetTreeNodeTextColor(const HierarchySystem *hs, EID e) {
	if (! hs->isEnabledInTree(e)) {
		return NODE_TEXT_COLOR_DISABLED;
	}
	if (hs->isPausedInTree(e)) {
		return NODE_TEXT_COLOR_PAUSED;
	}
	if (! hs->isVisibleInTree(e)) {
		return NODE_TEXT_COLOR_INVISIBLE;
	}
	return NODE_TEXT_COLOR_NORMAL;
}

void HierarchyInspector::onGuiTree(EID e) {
#ifndef NO_IMGUI
	if (!tree_->_getTreeNode(e)) return;

	bool has_hierarchy = (tree_->_getTreeNode(e) != NULL);
	int child_count = tree_->getChildCount(e);
	// 画面表示用の名前
	Path name;
	if (tree_->getName(e).empty()) {
		name = Path::fromFormat("#entity%d", e);
	} else {
		name = tree_->getName(e);
	}
	// ツリーラベル
	Path label;
	if (child_count > 0) {
		label = name.joinString(Path::fromFormat("[%d]", child_count));
	} else if (has_hierarchy) {
		label = name;
	} else {
		label = name.joinString(" (non hierarchical)");
	}
	// 再帰的に子ノードを追加していく
	ImGui::PushID(e);
	if (enable_mini_ctrl_) {
		{
			bool val = tree_->isEnabled(e);
			if (GUI_PutTreeNodeIcon(2, val, "Enable/Disable")) {
				tree_->setEnabled(e, !val);
			}
		}
		ImGui::SameLine();
		{
			bool val = tree_->isPaused(e);
			if (GUI_PutTreeNodeIcon(1, val, "Tick/Pause")) {
				tree_->setPaused(e, !val);
			}
		}
		ImGui::SameLine();
		{
			bool val = tree_->isVisible(e);
			if (GUI_PutTreeNodeIcon(0, val, "Show/Hide")) {
				tree_->setVisible(e, !val);
			}
		}
		ImGui::SameLine();
	}
	ImGui::PopID();

	bool selected = isSelected(e); // ノードが選択されている？
	ImVec4 text_color = GUI_GetTreeNodeTextColor(tree_, e);

	// ツリーノードの属性を決める
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnDoubleClick|ImGuiTreeNodeFlags_OpenOnArrow;
	if (selected) flags |= ImGuiTreeNodeFlags_Selected;
	if (child_count == 0) flags |= ImGuiTreeNodeFlags_Leaf;

	ImGui::PushStyleColor(ImGuiCol_Text, text_color);
	bool tree_expanded = ImGui::TreeNodeEx(ImGui_ID(e), flags, label.u8());
	ImGui::PopStyleColor();

	tmp_visible_tree_entities_.push_back(e);

	// ノードにマウスポインタが重なったらハイライト
	if (ImGui::IsItemHovered()) {
		highlight_entity_ = e;
	}

	// ノードがクリックされたら選択
	if (ImGui::IsItemClicked()) {
		if (ImGui::GetIO().KeyCtrl) {
			// Ctrl キーとともに押された。選択状態をトグル＆複数選択
			if (selected) {
				unselectEntity(e);
			} else {
				selectEntity(e);
			}
			range_selection_start_ = e; // 次に範囲選択が行われた場合、ここが起点となる
		
		} else if (ImGui::GetIO().KeyShift) {
			// Shift キーとともに押された。範囲選択する
			range_selection_end_ = e;
		
		} else if (ImGui::GetIO().KeyAlt) {
			// Alt キーとともに押された

		} else {
			// 修飾キーなしでクリックされている。単一選択する
			clearSelection(true);
			selectEntity(e);
			range_selection_start_ = e; // 次に範囲選択が行われた場合、ここが起点となる
		}
	}
	if (! tree_expanded) {
		// ツリービューを開いていない
		return;
	}

	if (has_hierarchy) {
		// 子ノードリスト
		std::vector<EID> sublist(child_count);
		for (int i=0; i<child_count; i++) {
			sublist[i] = tree_->getChild(e, i);
		}
		// 再帰的に子ノードを表示
		for (size_t i=0; i<sublist.size(); i++) {
			onGuiTree(sublist[i]);
		}
	}
	ImGui::TreePop();
#endif // !NO_IMGUI
}
#pragma endregion // HierarchyInspector


class CSortPred {
	IChildrenSoatingPred &user_pred_;
public:
	CSortPred(IChildrenSoatingPred &pred): user_pred_(pred) {
	}
	bool operator()(HierarchySystem::Node *n0, HierarchySystem::Node *n1) const {
		EID e0 = n0->e;
		EID e1 = n1->e;
		if (user_pred_.compare(e0, e1)) {
			K_assert(user_pred_.compare(e1, e0) == false);
			return true;
		}
		return false;
	}
};



#pragma region HierarchySystem

HierarchySystem::Node::Node() {
	e = 0;
	parent = (Node *)0; // NULL
	traversal_index = 0;
	traversal_first = 0;
	traversal_second = 0;
	flags = 0;
	layer = L_PARENT;
	tag = 0;
	rot_ = Quat();
}


HierarchySystem::HierarchySystem() {
	zeroClear();
}
HierarchySystem::~HierarchySystem() {
	onEnd();
}
void HierarchySystem::zeroClear() {
	tags_.clear();
	nodes_.clear();
	defined_tags_.clear();
	ordered_nodes_.clear();
	inspector_ = NULL;
	engine_ = NULL;
	tmp_traversal_index_ = 0;
	tmp_traversal_order_ = 0;
	dirty_traversal_index_ = false;
}
void HierarchySystem::init(Engine *engine) {
	Log_trace;
	Log_assert(engine);
	zeroClear();
	engine_ = engine;
	engine_->addSystem(this);
	engine_->addScreenCallback(this);
}
void HierarchySystem::onStart() {
	Inspector *ins = engine_->getInspector();
	if (ins) {
		inspector_ = new HierarchyInspector(this, ins);
		inspector_->onStart();
	}
}
void HierarchySystem::onEnd() {
	if (engine_) {
		engine_->removeAll();
	}
	if (inspector_) {
		delete inspector_;
		inspector_ = NULL;
	}
	zeroClear();
}
void HierarchySystem::attach(EID e) {
	if (isAttached(e)) return;

	// 新しいノードを作成
	Node node;
	node.e = e;
	node.parent = NULL;
	node.traversal_index = -1;
	node.traversal_first = -1;
	node.traversal_second = -1;
	node.flags = F_ENABLED | F_VISIBLE;
	node.layer = L_PARENT;
	node.scale_ = Vec3(1, 1, 1);
	node.independent_ = false;
	node.dirty_ = DIRTY_ALL;
	nodes_[e] = node;

	Node *pNode = &nodes_[e];

	// オブジェクトの作成順序を記録
	ordered_nodes_.push_back(pNode);

	// まだ親は設定されていない。ルートオブジェクトとして登録する
	roots_.push_back(pNode);
}
bool HierarchySystem::onAttach(EID e, const char *type) {
	if (type==NULL || strcmp(type, "Hierarchy") == 0 || strcmp(type, "Transform") == 0) {
		attach(e);
		return true;
	}
	return false;
}
void HierarchySystem::onDetach(EID e) {
#ifdef _DEBUG
	selfCheck();
#endif
	Node *node = _getTreeNode(e);
	if (node == NULL) return;

	if (inspector_) {
		inspector_->unselectEntity(e);
	}
	for (auto it=node->children.begin(); it!=node->children.end(); ++it) {
		Node *child = *it;

#ifdef _DEBUG
		// child はルートオブジェクトであるはずがない
		Log_assert(getRootIndex(child->e) < 0);
#endif

		// ここで子を親から切り離す。
		// 整合性を保つために、親から切り離された子はルートとして再登録する
		child->parent = NULL;
		roots_.push_back(child);
	}
	node->children.clear();
	_setParent(node, NULL);
	{
		auto it = std::find(ordered_nodes_.begin(), ordered_nodes_.end(), node);
		if (it != ordered_nodes_.end()) ordered_nodes_.erase(it);
	}
	{
		auto it = std::find(roots_.begin(), roots_.end(), node);
		if (it != roots_.end()) roots_.erase(it);
	}
	// エンティティを削除
	nodes_.erase(e);

	// 再インデックスが必要
	dirtyTraversalIndex();

#ifdef _DEBUG
	selfCheck();
#endif
}
bool HierarchySystem::setParameter(EID e, const char *key, const char *val) {
	Node *node = _getTreeNode(e);
	if (node == NULL) {
		return false;
	}
	if (strcmp(key, "pos.x") == 0) {
		Vec3 pos = getPosition(e); pos.x = K_strtof(val); setPosition(e, pos);
		return true;
	}
	if (strcmp(key, "pos.y") == 0) {
		Vec3 pos = getPosition(e); pos.y = K_strtof(val); setPosition(e, pos);
		return true;
	}
	if (strcmp(key, "pos.z") == 0) {
		Vec3 pos = getPosition(e); pos.z = K_strtof(val); setPosition(e, pos);
		return true;
	}
	if (strcmp(key, "scale.x") == 0) {
		Vec3 sca = getScale(e); sca.x = K_strtof(val); setScale(e, sca);
		return true;
	}
	if (strcmp(key, "scale.y") == 0) {
		Vec3 sca = getScale(e); sca.y = K_strtof(val); setScale(e, sca);
		return true;
	}
	if (strcmp(key, "scale.z") == 0) {
		Vec3 sca = getScale(e); sca.z = K_strtof(val); setScale(e, sca);
		return true;
	}
	if (strcmp(key, "independent") == 0) {
		setIndependent(e, K_strtoi(val) != 0);
		return true;
	}
	return false;
}
void HierarchySystem::dirtyAllNodes() {
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		Node &node = it->second;
		node.dirty_ |= DIRTY_WORLD;
	}
}
void HierarchySystem::onFrameUpdate() {
	// Transform 自体に変更が無くても、親子関係などが変更されてワールド座標が変化する可能性があるので、
	// 全ノードの world matrix を dirty にしておく
	dirtyAllNodes();
}
void HierarchySystem::onFrameEnd() {
	if (engine_) engine_->removeInvalidated(); // 無効化されたエンティティを削除
	dirtyTraversalIndex();
}

void HierarchySystem::defineTag(int tag, const Path &name) {
	if (tag == 0) {
		Log_error("E_TAG: tag 0 is reserved.");
		return;
	}
	tags_[tag] = name;
}
int HierarchySystem::getTagByName(const Path &name) {
	for (auto it=tags_.begin(); it!=tags_.end(); ++it) {
		if (it->second == name) {
			return it->first;
		}
	}
	return 0;
}
bool HierarchySystem::isTagDefined(int tag) const {
	if (tag) return tags_.find(tag) != tags_.end();
	return false;
}
void HierarchySystem::onEntityInspectorGui(EID e) {
	if (inspector_) inspector_->onGuiEntity(e);
}
void HierarchySystem::onInspectorGui()  {
	if (inspector_) inspector_->onGuiMain();
}




#pragma region ノード
HierarchySystem::Node * HierarchySystem::_getTreeNode(EID e) {
	if (e == NULL) return NULL;
	auto it = nodes_.find(e);
	if (it == nodes_.end()) return NULL;
	return &it->second;
}
const HierarchySystem::Node * HierarchySystem::_getTreeNode(EID e) const {
	if (e == NULL) return NULL;
	auto it = nodes_.find(const_cast<EID>(e));
	if (it == nodes_.end()) return NULL;
	return &it->second;
}
int HierarchySystem::numAllEntities() const {
	return (int)nodes_.size();
}
void HierarchySystem::getAllEntities(EntityList &list) const {
	list.clear();
	list.reserve(nodes_.size());
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		list.push_back(it->first);
	}
}
#pragma endregion


#pragma region 構造
bool HierarchySystem::exists(EID e) const {
	return _getTreeNode(e) != NULL;
}
class _Invalidator: public IHierarchyWalker {
public:
	Engine *engine_;

	virtual bool onVisit(EID e) override {
		engine_->invalidate(e);
		return true;
	}
};
void HierarchySystem::removeTree(EID e, bool remove_self) {
	if (e == NULL) return;
	// e の子をすべて無効化する
	_Invalidator inv;
	inv.engine_ = engine_;
	scanChildren(e, inv);

	// e を無効化する
	if (remove_self) {
		engine_->invalidate(e);
	}
}
void HierarchySystem::_setParent(Node *node, Node *parent) {
	Log_assert(node);

	// 既存の親子関係を解除する
	if (node->parent) {
		// 親から切り離す
		int idx = getChildIndex(node->parent, node);
		_removeChildAt(node->parent, idx);
		Log_assert(node->parent == NULL);
	} else {
		// ルートから外す
		for (auto it=roots_.begin(); it!=roots_.end(); ++it) {
			if (*it == node) {
				roots_.erase(it);
				break;
			}
		}
	}

	// 親子関係を設定する
	if (parent) {
		// 新しい親とつなげる
		node->parent = parent;
		parent->children.push_back(node);
	} else {
		// ルートとして追加する
		node->parent = NULL;
		roots_.push_back(node);
	}
	dirtyTraversalIndex();
}
void HierarchySystem::_removeChildAt(Node *node, int index) {
	Log_assert(node);
	if (index < 0 || (int)node->children.size() <= index) return;
	node->children[index]->parent = NULL; // 再帰処理になってしまうため co->SetParent(NULL) は使わない
	node->children.erase(node->children.begin() + index);
	dirtyTraversalIndex();
}
void HierarchySystem::sortChildren(EID e, IChildrenSoatingPred &pred) {
	if (e == NULL) return;
	Node *node = _getTreeNode(e);
	if (node == NULL) return;
	if (node->children.size() <= 1) return;

	CSortPred sp(pred);
	std::sort(node->children.begin(), node->children.end(), sp);
	dirtyTraversalIndex();
}
#pragma endregion


#pragma region 構造変更（間接）
void HierarchySystem::setParent(EID e, EID parent) {
	if (e == NULL) return;
	Node *node = _getTreeNode(e);
	Node *parent_n = _getTreeNode(parent);
	if (parent && parent_n == NULL) {
		// 未登録の parent が指定された
		Log_errorf("E_INVALID_CHILD_ADDING: parent entity does not have a hierarchy component: Parent=0x%x", parent);
		return;
	}
	if (node == NULL) {
		// NULL を追加することはできない
		Log_error("E_INVALID_CHILD_ADDING: child entity does not have a hierarchy component");
		return;
	}
	_setParent(node, parent_n);
}
void HierarchySystem::moveChild(EID e, int index_from, int index_to) {
	Node *node = _getTreeNode(e);
	if (node) moveChild(node, index_from, index_to);
}
void HierarchySystem::moveChild(Node *node, int index_from, int index_to) {
	Log_assert(node);
	if (0 <= index_from && index_from < (int)node->children.size()) {
		
		// ノードを外す
		Node *sub = node->children[index_from];
		node->children.erase(node->children.begin() + index_from);

		// 新しい位置へ挿入
		if (index_to < 0) {
			node->children.insert(node->children.begin(), sub);

		} else if (index_to < (int)node->children.size()) {
			node->children.insert(node->children.begin() + index_to, sub);

		} else {
			node->children.push_back(sub);
		}

		// 再インデックス
		dirtyTraversalIndex();
	}
}
void HierarchySystem::moveToFront(EID e) {
	EID parent = getParent(e);
	if (parent) {
		int idx = getChildIndex(parent, e);
		moveChild(parent, idx, 0);
	}
}
void HierarchySystem::moveToBack(EID e) {
	EID parent = getParent(e);
	if (parent) {
		int idx = getChildIndex(parent, e);
		int num = getChildCount(parent);
		moveChild(parent, idx, num-1);
	}
}
#pragma endregion


#pragma region 名前
void HierarchySystem::setName(EID e, const Path &name) {
	Node *node = _getTreeNode(e);
	if (node == NULL) return;
	#ifdef _DEBUG
	{
		// ここでセットする名前は、パス区切りを含んでいてはいけない。
		if (!name.directory().empty()) { // <-- name がパス区切りを含んでいると directory() が empty にならない
			Log_errorf("E_INVALID_ENTITY_NAME: %s", name.u8());
		}
	}
	#endif
	node->name = name;
}
bool HierarchySystem::hasName(EID e, const Path &name) const {
	return getName(e).compare(name) == 0;
}
const Path & HierarchySystem::getName(EID e) const {
	const Node *node = _getTreeNode(e);
	if (node) {
		return node->name;
	}
	return Path::Empty; // アタッチされていない
}
Path HierarchySystem::getNameInTree(EID e) const {
	const Node *node = _getTreeNode(e);
	if (node) {
		// 親をたどりながらパスを結合していく
		Path path;
		while (node) {
			path = node->name.join(path);
			node = node->parent;
		}
		return path;
	}
	return Path::Empty; // アタッチされていない
}
bool HierarchySystem::isNameMatched(EID e, const Path &subpath) const {
	const Path &name = getName(e);
	// エンティティのパスが name_path で終わっているか調べる
	if (subpath.empty()) {
		return name.empty();
	}
	// エンティティパスの末尾要素（つまりエンティティの名前）と name_path の末尾要素が一致しているか確認
	if (name.compare(subpath.filename()) != 0) {
		if (name.compare(subpath.filename(), true, true) == 0) {
			Log_warningf(u8"大小文字だけが異なるエンティティがあります。'%s' を探していますが '%s' が存在します", subpath.u8(), name.u8());
		}
		return false;
	}
	// 一致したので正確なパスで後方一致を調べる
	const Path &name_path = getNameInTree(e); // <-- この呼び出しのコストが高いことに注意
	if (! name_path.endsWithPath(subpath)) {
		return false;
	}
	return true; // 一致した
}
#pragma endregion


#pragma region 検索
EID HierarchySystem::findEntity(const Path &name) const {
	if (name.empty()) return NULL;
	for (auto it=ordered_nodes_.begin(); it!=ordered_nodes_.end(); ++it) {
		const Node *node = *it;
		if (node->name.compare(name) == 0) {
			return node->e;
		}
	}
	return NULL;
}
EID HierarchySystem::findEntityBack(const Path &name) const {
	if (name.empty()) return NULL;
	for (auto it=ordered_nodes_.rbegin(); it!=ordered_nodes_.rend(); ++it) {
		const Node *node = *it;
		if (node->name.compare(name) == 0) {
			return node->e;
		}
	}
	return NULL;
}
EID HierarchySystem::findEntityByNamePath(const Path &name_path) {
	if (name_path.empty()) return NULL;
	for (auto it=ordered_nodes_.begin(); it!=ordered_nodes_.end(); ++it) {
		const Node *node = *it;
		if (isNameMatched(node->e, name_path)) {
			return node->e;
		}
	}
	return NULL;
}
int HierarchySystem::findMultipleEntitiesByNamePath(const Path &name_path, EntityList *multiple_result) {
	if (name_path.empty()) return NULL;
	int num = 0;
	for (auto it=ordered_nodes_.begin(); it!=ordered_nodes_.end(); ++it) {
		const Node *node = *it;
		if (isNameMatched(node->e, name_path)) {
			if (multiple_result) {
				multiple_result->push_back(node->e);
			}
			num++;
		}
	}
	return num;
}
EID HierarchySystem::findChildByNameExact(EID e, const Path &name, bool recursive) {
	const Node *curr_n = _getTreeNode(e);
	if (curr_n == NULL) {
		return NULL;
	}
	for (auto it=curr_n->children.begin(); it!=curr_n->children.end(); ++it) {
		Node *child = *it;
		Log_assert(child);
		if (hasName(child->e, name)) {
			return child->e;
		}
		if (recursive) {
			EID ce = findChildByNameExact(child->e, name, true);
			if (ce) return ce;
		}
	}
	return NULL;
}
#pragma endregion


#pragma region レイヤ
void HierarchySystem::setLayer(EID e, Layer layer) {
	Node *node = _getTreeNode(e);
	if (node) node->layer = layer;
}
HierarchySystem::Layer HierarchySystem::getLayer(EID e) const {
	const Node *node = _getTreeNode(e);
	return node ? node->layer : L_DEFAULT;
}
HierarchySystem::Layer HierarchySystem::getLayerInTree(EID e) const {
	const Node *node = _getTreeNode(e);
	if (node == NULL) return L_DEFAULT;
	if (node->layer != L_PARENT) return node->layer;
	if (node->parent == NULL) return L_DEFAULT;
	Layer lay = getLayerInTree(node->parent->e);
	Log_assert(lay >= 0);
	return lay;
}
HierarchySystem::Layer HierarchySystem::getLayerInTree(const Node *node) const {
	Log_assert(node);
	if (node->layer != L_PARENT) return node->layer;
	if (node->parent == NULL) return L_DEFAULT;
	Layer lay = getLayerInTree(node->parent);
	Log_assert(lay >= 0);
	return lay;
}
#pragma endregion


#pragma region タグ
void HierarchySystem::setTag(EID e, int tag) {
	Node *node = _getTreeNode(e);
	if (node) node->tag = tag;
}
int HierarchySystem::getTag(EID e) const {
	const Node *node = _getTreeNode(e);
	return node ? node->tag : 0;
}
int HierarchySystem::getTagInTree(EID e) const {
	const Node *node = _getTreeNode(e);
	return node ? getTagInTree(node) : 0;
}
int HierarchySystem::getTagInTree(const Node *node) const {
	Log_assert(node);
	if (node->tag) return node->tag;
	if (node->parent == NULL) return 0;
	return getTagInTree(node->parent);
}
bool HierarchySystem::hasTag(EID e, int tag) const {
	const Node *node = _getTreeNode(e);
	if (node) {
		return node->tag == tag;
	}
	return false;
}
bool HierarchySystem::hasTagInTree(EID e, int tag) const {
	const Node *node = _getTreeNode(e);
	if (node) {
		return hasTagInTree(node, tag);
	}
	return false;
}
bool HierarchySystem::hasTagInTree(const Node *node, int tag) const {
	const Node *n = node;
	while (n) {
		if (n->tag == tag) return true;
		n = n->parent;
	}
	return false;
}
#pragma endregion


#pragma region フラグ
void HierarchySystem::setFlag(EID e, Flag flag, bool value) {
	Node *node = _getTreeNode(e);
	if (node) {
		setFlag(node, flag, value);
	}
}
void HierarchySystem::setFlag(Node *node, Flag flag, bool value) {
	Log_assert(node);
	if (value) {
		node->flags |= flag;
	} else {
		node->flags &= ~flag;
	}
}
bool HierarchySystem::hasFlag(EID e, Flag flag) const {
	const Node *node = _getTreeNode(e);
	if (node) {
		if (hasFlag(node, flag)) {
			return true;
		}
	}
	return false;
}
bool HierarchySystem::hasFlag(const Node *node, Flag flag) const {
	Log_assert(node);
	return (node->flags & flag) != 0;
}
bool HierarchySystem::hasFlagInTreeAnd(EID e, Flag flag) const {
	const Node *node = _getTreeNode(e);
	return hasFlagInTreeAnd(node, flag);
}
bool HierarchySystem::hasFlagInTreeOr(EID e, Flag flag) const {
	const Node *node = _getTreeNode(e);
	return hasFlagInTreeOr(node, flag);
}
bool HierarchySystem::hasFlagInTreeAnd(const Node *node, Flag flag) const {
	// ルートまでの全ての親のフラグが true の場合のみ、自分も true になる
	while (node) {
		if (hasFlag(node, flag)) {
			node = node->parent;
		} else {
			return false;
		}
	}
	return true;
}
bool HierarchySystem::hasFlagInTreeOr(const Node *node, Flag flag) const {
	// ルートまでのどれか一つの親のフラグが true ならば、自分も true になる
	while (node) {
		if (hasFlag(node, flag)) {
			return true;
		} else {
			node = node->parent;
		}
	}
	return false;
}
#pragma endregion


#pragma region 順序
void HierarchySystem::dirtyTraversalIndex() {
	dirty_traversal_index_ = true;
}
void HierarchySystem::getTraversalIndex(EID e, int *order, int *first, int *second) const {
	const Node *node = _getTreeNode(e);
	if (node) getTraversalIndex(node, order, first, second);
}
void HierarchySystem::getTraversalIndex(const Node *node, int *index, int *first, int *second) const {
	Log_assert(node);
	if (dirty_traversal_index_) {
		// 巡回番号が古くなっている。更新する
		tmp_traversal_index_ = 0;
		tmp_traversal_order_ = 0;
		for (auto it=ordered_nodes_.begin(); it!=ordered_nodes_.end(); ++it) {
			Node *n = *it;
			if (n->parent == NULL) {
				_traverse(n); // 親のいないノードだけを巡回の起点にする
			}
		}
		dirty_traversal_index_ = false;
	}
	if (index) *index = node->traversal_index;
	if (first) *first = node->traversal_first;
	if (second)*second= node->traversal_second;
}
void HierarchySystem::_traverse(Node *node) const {
	Log_assert(node);
	// ツリー内を重複しなよう数えたときの通し番号
	node->traversal_index = tmp_traversal_index_++;

	if (node->children.empty()) {
		// 子ノードがない。
		// 行きと帰りのインデックスは同じ
		node->traversal_first  = tmp_traversal_order_;
		node->traversal_second = tmp_traversal_order_;
		tmp_traversal_order_++;
	} else {
		// 子ノードがある。
		// 行きのインデックスを記録し、子を巡回し、帰りのインデックスを記録する
		node->traversal_first = tmp_traversal_order_++;
		for (auto it=node->children.begin(); it!=node->children.end(); ++it) {
			_traverse(*it);
		}
		node->traversal_second = tmp_traversal_order_++;
	}
}
int HierarchySystem::getSerialIndex(EID e) {
	int ord = 0;
	getTraversalIndex(e, &ord, NULL, NULL);
	return ord;
}
#pragma endregion


#pragma region 巡回
void HierarchySystem::_scan(Node *node, IHierarchyWalker &walker, int tag) {
	K_assert(node);
	for (auto it=node->children.begin(); it!=node->children.end(); ++it) {
		Node *child = *it;
		if (tag==0 || child->tag == tag) {
			if (!walker.onVisit(child->e)) {
				break;
			}
		}
		_scan(child, walker, tag);
	}
}
void HierarchySystem::scanChildren(EID e, IHierarchyWalker &walker, int tag) {
	Node *node = _getTreeNode(e);
	if (node) {
		_scan(node, walker, tag);
	}
}
void HierarchySystem::scanParents(EID e, IHierarchyWalker &walker) {
	Node *node = _getTreeNode(e);
	if (node) {
		for (auto it=node->parent; it!=NULL; it=it->parent) {
			walker.onVisit(it->e);
		}
	}
}
#pragma endregion


#pragma region ルート要素
int HierarchySystem::getRootCount() const {
	return (int)roots_.size();
}
EID HierarchySystem::getRoot(int index) const {
	if (0 <= index && index < (int)roots_.size()) {
		return roots_[index]->e;
	}
	return NULL;
}
int HierarchySystem::getRootIndex(EID root) const {
	for (size_t i=0; i<roots_.size(); i++) {
		const Node *node = roots_[i];
		if (node->e == root) {
			return i;
		}
	}
	return -1;
}
int HierarchySystem::getRootIndex(const Path &name) const {
	for (size_t i=0; i<roots_.size(); i++) {
		const Node *node = roots_[i];
		if (node->name.compare(name) == 0) {
			return (int)i;
		}
	}
	return -1;
}
void HierarchySystem::moveRoot(int index_from, int index_to) {
	if (0 <= index_from && index_from < (int)roots_.size()) {

		// ノードを外す
		Node *node = roots_[index_from];
		roots_.erase(roots_.begin() + index_from);

		// 新しい位置へ挿入
		if (index_to < 0) {
			roots_.insert(roots_.begin(), node);
		
		} else if (index_to < (int)roots_.size()) {
			roots_.insert(roots_.begin() + index_to, node);

		} else {
			roots_.push_back(node);
		}

		// 再インデックス
		dirtyTraversalIndex();
	}
}
#pragma endregion


#pragma region 子要素
int HierarchySystem::getChildCount(EID entity) const {
	const Node *node = _getTreeNode(entity);
	return node ? (int)node->children.size() : 0;
}
EID HierarchySystem::getChild(EID e, int index) const {
	const Node *node = _getTreeNode(e);
	if (node == NULL) return NULL;
	if (index < 0) return NULL;
	if (index >= (int)node->children.size()) return NULL;
	return node->children[index]->e;
}
int HierarchySystem::getChildIndex(EID e, EID child) const {
	const Node *parent_n = _getTreeNode(e);
	const Node *child_n = _getTreeNode(child);
	return getChildIndex(parent_n, child_n);
}
int HierarchySystem::getChildIndex(EID e, const Path &name) const {
	const Node *node = _getTreeNode(e);
	if (node) {
		for (size_t i=0; i<node->children.size(); i++) {
			const Node *sub = node->children[i];
			if (sub->name.compare(name) == 0) {
				return (int)i;
			}
		}
	}
	return -1;
}
int HierarchySystem::getChildIndex(const Node *parent, const Node *child) const {
	Log_assert(child);
	if (parent == NULL) {
		// parent が NULL の場合はルートの子インデックスを返す
		int idx = 0;
		for (auto it=ordered_nodes_.begin(); it!=ordered_nodes_.end(); ++it) {
			Node *node = *it;
			if (node->parent) continue; // ルート要素でないものはスキップ
			if (node == child) return idx;
			idx++;
		}
		return -1; // NOT FOUND

	} else {
		for (int i=0; i<(int)parent->children.size(); i++) {
			Node *co = parent->children[i];
			if (co == child) return i;
		}
		return -1; // NOT FOUND
	}
}
#pragma endregion



#pragma region 親子
EID HierarchySystem::getParent(EID entity) const {
	const Node *node = _getTreeNode(entity);
	if (node && node->parent) {
		return node->parent->e;
	}
	return NULL;
}
bool HierarchySystem::isAncestorOf(EID e, EID descendabt) const {
	EID parent = getParent(descendabt);
	while (parent) {
		if (parent == e) return true;
		parent = getParent(parent);
	}
	return false;
}
#pragma endregion


#pragma region Enabled
void HierarchySystem::setEnabled(EID e, bool val) {
	setFlag(e, F_ENABLED, val);
}
bool HierarchySystem::isEnabled(EID e) const {
	return hasFlag(e, F_ENABLED);
}
bool HierarchySystem::isEnabledInTree(EID e) const {
	return hasFlagInTreeAnd(e, F_ENABLED);
}
#pragma endregion


#pragma region Paused
void HierarchySystem::setPaused(EID e, bool val) {
	setFlag(e, F_PAUSED, val);
}
bool HierarchySystem::isPaused(EID e) const {
	return hasFlag(e, F_PAUSED);
}
bool HierarchySystem::isPausedInTree(EID e) const {
	return hasFlagInTreeOr(e, F_PAUSED);
}
#pragma endregion


#pragma region Visible
void HierarchySystem::setVisible(EID e, bool val) {
	setFlag(e, F_VISIBLE, val);
}
bool HierarchySystem::isVisible(EID e) const {
	return hasFlag(e, F_VISIBLE);
}
bool HierarchySystem::isVisibleInTree(EID e) const {
	return hasFlagInTreeAnd(e, F_VISIBLE);
}
#pragma endregion


void HierarchySystem::onDrawDebugInfo(const DebugRenderParameters &p) {
#ifndef NO_IMGUI
	Inspector *inspector = engine_->getInspector();
	if (inspector == NULL) return;

	// インスペクター上でシステムペインが開いているか、デバッグ表示が ON になっていればデバッグ情報を描画する
	bool show_debug = false;
	SystemEditorParams *info = inspector->getSystemEditorParams(this);
	if (inspector->isVisible()) {
		show_debug = info->is_open;
	} else {
		show_debug = info->always_show_debug;
	}
	if (!show_debug) {
		return; // 描画しない
	}

	Screen *screen = engine_->getScreen();
	Log_assert(screen);

	RenderSystem *rensys = engine_->findSystem<RenderSystem>();
	Log_assert(rensys);

	const float AXIS_LEN = 32;
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		EID e = it->first;
		const Node *node = &it->second;
		// カメラのレイヤーと一致しないと描画対象にならない
		int layer_this = getLayerInTree(&it->second);
		if (layer_this != p.view.object_layer) {
			continue;
		}

		// インスペクター上で選択またはハイライト状態になっていないとダメ
		if (!inspector->isEntitySelected(e) && !inspector->isEntityHighlighted(e, false)) {
			continue;
		}
		// 座標軸を描画する
		Matrix4 m = _computeWorldMatrix(node) * p.view.transform_matrix;
		p.gizmo->addAxis(m, Vec3(), AXIS_LEN);
	}
#endif // !NO_IMGUI
}
void HierarchySystem::copyTransform(EID dst_e, EID src_e) {
	Node *dst = _getTreeNode(dst_e);
	Node *src = _getTreeNode(src_e);
	if (dst && src) {
		dst->pos_         = src->pos_;
		dst->scale_       = src->scale_;
		dst->rot_         = src->rot_;
		dst->independent_ = src->independent_;
		dst->pivot_       = src->pivot_;
		dst->dirty_ = DIRTY_ALL;
	}
}
void HierarchySystem::_computeLocalMatrix(const Node *node, Matrix4 *m, Matrix4 *inv_m) const{
	Log_assert(node);
	if (node->dirty_ & DIRTY_LOCAL) {
		const Vec3 &pos = node->pos_;
		const Vec3 &scale = node->scale_;
		const Quat &rot = node->rot_;
		Matrix4 Tr = Matrix4::fromTranslation(pos);
		Matrix4 Ro = Matrix4::fromRotation(rot);
		Matrix4 Sc = Matrix4::fromScale(scale);
		Matrix4 inv_Tr = Matrix4::fromTranslation(-pos);
		Matrix4 inv_Ro = Matrix4::fromRotation(rot.inverse());
		Matrix4 inv_Sc = Matrix4::fromScale(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);
		Matrix4 Pv = Matrix4::fromTranslation(node->pivot_);
		Matrix4 inv_Pv = Matrix4::fromTranslation(-node->pivot_);
		node->local_matrix_ = inv_Pv * Sc * Ro * Pv * Tr; //<-- 拡大回転 SR を ピヴォットの平行移動 inv_P, P で挟む
		node->local_matrix_inversed_ = inv_Tr * inv_Pv * inv_Ro * inv_Sc * Pv;
		node->dirty_ &= ~DIRTY_LOCAL;
	}
	if (m) *m = node->local_matrix_;
	if (inv_m) *inv_m = node->local_matrix_inversed_;
}

/// node のワールド座標を計算するために必要な親ノードを返す。
/// 通常はヒエラルキー上の親をそのまま返すが、node の座標系が親から独立していたり、
/// 親が Transform 対象外であるなど、計算に不要な場合は NULL を返す
const HierarchySystem::Node * HierarchySystem::_getLinkedParent(const Node *node) const {
	Log_assert(node);

	// 親子関係から独立している場合、ワールド座標はローカル座標と同じになる
	if (node->independent_) {
		return NULL;
	}

	// 親が存在しない場合、子のワールド座標はローカル座標と同じになる
	if (node->parent == NULL) {
		return NULL;
	}

	// ワールド座標するためには親の座標が必要になる
	return node->parent;
}
Matrix4 HierarchySystem::_computeWorldMatrix(const Node *node) const {
	Log_assert(node);

	if (node->dirty_ & DIRTY_WORLD) {
		// 自分のローカル行列
		Matrix4 local_m;
		_computeLocalMatrix(node, &local_m, NULL);

		// 必要ならば親のワールド行列を得る
		const Node *parent_node = _getLinkedParent(node);
		if (parent_node) {
			// node のワールド座標は親の影響を受ける。
			// 親のワールド座標と合成する必要がある
			Matrix4 parent_m = _computeWorldMatrix(parent_node);
			node->world_matrix_ = local_m * parent_m;

		} else {
			// node のワールド座標は親の影響を受けない。
			// ワールド座標はローカル座標で代用できる
			node->world_matrix_ = local_m;
		}
		node->dirty_ &= ~DIRTY_WORLD;
	}
	return node->world_matrix_;
}
void HierarchySystem::getMatrix(EID e, Matrix4 *out) {
	const Node *node = _getTreeNode(e);
	if (node) _computeLocalMatrix(node, out, NULL);
}
void HierarchySystem::getMatrixInversed(EID e, Matrix4 *out) {
	const Node *node = _getTreeNode(e);
	if (node) _computeLocalMatrix(node, NULL, out);
}
bool HierarchySystem::getLocalToWorldMatrix(EID e, Matrix4 *out) const {
	const Node *node = _getTreeNode(e);
	if (node) {
		if (out) *out = _computeWorldMatrix(node);
		return true;
	}
	return false;
}
bool HierarchySystem::getWorldToLocalMatrix(EID e, Matrix4 *out) const {
	const Node *node = _getTreeNode(e);
	if (node) {
		Matrix4 m;
		while (node) {
			Matrix4 tm;
			_computeLocalMatrix(node, NULL, &tm);
			m = tm * m;
			if (node->independent_) {
				break; // これ以上親をたどらない
			}
			node = node->parent;
		}
		if (out) *out = m;
		return true;
	}
	return false;
}
Vec3 HierarchySystem::localToWorldPoint(EID e, const Vec3 &local) const {
	Matrix4 m;
	if (getLocalToWorldMatrix(e, &m)) {
		return m.transform(local);
	}
	return local;
}
Vec3 HierarchySystem::worldToLocalPoint(EID e, const Vec3 &world) const {
	Matrix4 m;
	if (getWorldToLocalMatrix(e, &m)) {
		return m.transform(world);
	}
	return world;
}

const Vec3 & HierarchySystem::getPosition(EID e) {
	static const Vec3 s_def;
	Node *node = _getTreeNode(e);
	return node ? node->pos_ : s_def;
}
Vec3 HierarchySystem::getPositionInWorld(EID e) {
	const Node *node = _getTreeNode(e);
	if (node) {
		Matrix4 m;
		getLocalToWorldMatrix(e, &m);
		return m.transformZero();
	}
	return Vec3();
}
const Vec3 & HierarchySystem::getScale(EID e) {
	static const Vec3 s_def(1, 1, 1);
	Node *node = _getTreeNode(e);
	return node ? node->scale_ : s_def;
}
const Vec3 & HierarchySystem::getPivot(EID e) {
	static const Vec3 s_def;
	Node *node = _getTreeNode(e);
	return node ? node->pivot_ : s_def;
}
void HierarchySystem::setPivot(EID e, const Vec3 &value) {
	Node *node = _getTreeNode(e);
	if (node) {
		node->pivot_ = value;
		node->dirty_ = DIRTY_ALL;
	}
}
const Quat & HierarchySystem::getRotation(EID e) {
	static const Quat s_def = Quat();
	Node *node = _getTreeNode(e);
	return node ? node->rot_ : s_def;
}
void HierarchySystem::setPosition(EID e, float x, float y, float z) {
	setPosition(e, Vec3(x, y, z));
}
void HierarchySystem::setPosition(EID e, const Vec3 &value) {
	Node *node = _getTreeNode(e);
	if (node) {
		// NaN や INF でないことを確認
		Log_assert(isfinite(value.x) && isfinite(value.y) && isfinite(value.z));
		node->pos_ = value;
		node->dirty_ = DIRTY_ALL;
	}
}
void HierarchySystem::setScale(EID e, float sx, float sy, float sz) {
	setScale(e, Vec3(sx, sy, sz));
}
void HierarchySystem::setScale(EID e, const Vec3 &value) {
	Node *node = _getTreeNode(e);
	if (node) {
		// NaN や INF でないことを確認
		Log_assert(isfinite(value.x) && isfinite(value.y) && isfinite(value.z));
		node->scale_ = value;
		node->dirty_ = DIRTY_ALL;
	}
}
void HierarchySystem::setRotation(EID e, const Quat &rot) {
	Node *node = _getTreeNode(e);
	if (node) {
		// NaN や INF でないことを確認
		Log_assert(isfinite(rot.x) && isfinite(rot.y) && isfinite(rot.z) && isfinite(rot.w));
		node->rot_ = rot;
		node->dirty_ = DIRTY_ALL;
	}
}
void HierarchySystem::setRotationIdentity(EID e) {
	setRotation(e, Quat());
}
void HierarchySystem::setRotation(EID e, const Vec3 &axis, float degrees) {
	setRotation(e, Quat(axis, degrees));
}
void HierarchySystem::setRotationZ(EID e, float degrees) {
	setRotation(e, Vec3(0, 0, 1), degrees);
}
void HierarchySystem::rotate(EID e, const Vec3 &axis, float degrees) {
	Quat qBase = getRotation(e);
	Quat qMore = Quat(axis, degrees);
	Quat qApply = qBase * qMore;
	setRotation(e, qApply);
}
void HierarchySystem::rotateZ(EID e, float degrees) {
	rotate(e, Vec3(0, 0, 1), degrees);
}

bool HierarchySystem::isIndependent(EID e) {
	Node *node = _getTreeNode(e);
	return node ? node->independent_ : false;
}
void HierarchySystem::setIndependent(EID e, bool value) {
	Node *node = _getTreeNode(e);
	if (node) {
		node->independent_ = value;
		node->dirty_ |= DIRTY_WORLD; // independent が変化した場合、ローカル座標は変化しないがワールド座標は変化する
	}
}

void HierarchySystem::selfCheck() {
	// 親が NULL なのにルートに登録されていないモノ
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		EID e = it->first;
		if (it->second.parent == NULL) {
			if (getRootIndex(e) < 0) {
				Path name = getNameInTree(e);
				Log_assert(0);
				removeTree(e);
			}
		}
	}

	// root にあるものは必ず　nodes 内にもある
	for (size_t i=0; i<roots_.size(); i++) {
		Node *node = roots_[i];
		bool ok = false;
#if 1
		auto it = nodes_.find(node->e);
		if (it != nodes_.end()) {
			if (&it->second == node) {
				ok = true;
			}
		}
#else
		for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
			if (&it->second == node) {
				ok = true;
				break;
			}
		}
#endif
		if (!ok) {
			Path name = getNameInTree(node->e);
			Log_assert(ok);
		}
	}

	Log_assert(ordered_nodes_.size() == nodes_.size());
}

#pragma endregion // HierarchySystem



void Test_entity_tree() {
	#if 0
	EntityTree tree;
	tree.init(NULL);
	EID e0 = tree.create(NULL);
	EID e1 = tree.create(NULL);
	EID e00 = tree.create(e0);
	K_assert(tree.getRoot(0) == e0);
	K_assert(tree.getRoot(1) == e1);
	K_assert(tree.getParent(e1) == NULL);
	K_assert(tree.getParent(e00) == e0);

	tree.removeTree(e0);
	K_assert(tree.exists(e0));
	K_assert(tree.exists(e00));

	tree.removeInvalidated();
	K_assert(tree.exists(e0) == false);
	K_assert(tree.exists(e00) == false);
	K_assert(tree.exists(e1));

	K_assert(tree.getRoot(0) == e1);

	tree.clear();
#endif
}

}

