#pragma once
#include <unordered_map>
#include <unordered_set>
#include "system.h"
#include "KPath.h"
#include "KQuat.h"
#include "Screen.h" // IScreenCallback

namespace mo {

class Engine;
class HierarchySystem;
class Inspector;
struct DebugRenderParameters;

class HierarchyInspector {
public:
	HierarchyInspector(HierarchySystem *tree, Inspector *ins);
	void onStart();
	bool isSelected(EID e);
	void selectEntity(EID e);
	void unselectEntity(EID e);
	void clearSelection(bool clearSelection=true);
	void onGuiEntity(EID e);
	void onGuiMain();
	void onGuiTree(EID e);

private:
	void movePrev();
	void moveNext();
	std::vector<EID> selected_entities_;
	std::vector<EID> range_selection_buf_;
	std::vector<EID> tmp_visible_tree_entities_; // インスペクターのツリービュー上に表示されているノード
	std::vector<EID> tmp_entities_;

	HierarchySystem *tree_;

	Inspector *main_inspector_;
	EID highlight_entity_;
	EID range_selection_start_;
	EID range_selection_end_;
	bool range_selection_required_;
	bool enable_mini_ctrl_;
};

class IHierarchyWalker {
public:
	virtual bool onVisit(EID e) = 0;
};

class IChildrenSoatingPred {
public:
	// std::sort 等の比較方式と同じ。この戻り値が全て true になるように並び変える
	virtual bool compare(EID e0, EID e1) { return true; }
};

class HierarchySystem: public System, public IScreenCallback {
public:
	enum Flag {
		F_ENABLED = 0x01,
		F_PAUSED  = 0x02,
		F_VISIBLE = 0x04,
	};
	typedef int Flags;

	enum Layer {
		L_PARENT = -1, // 親ノードから継承する
		L_DEFAULT,
		L_UI,
		L_USER,
	};

	enum DIRTY {
		DIRTY_LOCAL = 1,
		DIRTY_WORLD = 2,
		DIRTY_ALL   = 3,
	};

	struct Node {
		Node();
		EID e;
		Node *parent;
		std::vector<Node *> children;
		int traversal_index;
		int traversal_first;
		int traversal_second;
		Path name;
		Flags flags;
		Layer layer;
		int tag;

		/////////////////
		Vec3 pos_;
		Vec3 scale_;
		Vec3 pivot_;
		Quat rot_;
		mutable Matrix4 local_matrix_;
		mutable Matrix4 local_matrix_inversed_;
		mutable Matrix4 world_matrix_;
		mutable int dirty_;
		bool independent_;
	};

public:
	HierarchySystem();
	virtual ~HierarchySystem();
	void init(Engine *engine);
	void attach(EID e);

	virtual bool onAttach(EID e, const char *type) override;
	virtual void onStart() override;
	virtual void onEnd() override;
	virtual void onDetach(EID e) override;
	virtual bool onIsAttached(EID e) override { return _getTreeNode(e) != NULL; }
	virtual void onFrameUpdate() override;
	virtual void onFrameEnd() override;
	virtual void onInspectorGui() override;
	virtual void onEntityInspectorGui(EID e) override;
	virtual bool setParameter(EID e, const char *key, const char *val) override;

	//IScreenCallback
	virtual void onDrawDebugInfo(const DebugRenderParameters &p) override;

	///////////////////////////////////////////////////////
	void setName(EID e, const Path &name);
	bool hasName(EID e, const Path &name) const;
	const Path & getName(EID e) const;
	Path getNameInTree(EID e) const;
	EID findEntity(const Path &name) const;
	EID findEntityBack(const Path &name) const;

	/// 親子関係も含め、指定された名前と一致するもの探す。
	/// 例えば "xxx" と指定した場合、名前が "xxx" と一致するエンティティのうち、最初に見つかったものを返す。
	/// "xxx/yyy" と指定した場合、名前が "yyy" で親が "xxx" であるようなエンティティを返す。
	/// "xxx/yyy/zzz" なども上記法則にしたがう
	EID findEntityByNamePath(const Path &name_path);
	int findMultipleEntitiesByNamePath(const Path &name_path, EntityList *multiple_result);
	bool isNameMatched(EID e, const Path &name_path) const;
	/// 指定された名前と完全に一致する子を探す
	EID findChildByNameExact(EID e, const Path &name, bool recursive);


	///////////////////////////////////////////////////////
	void  setLayer(EID e, Layer layer);
	Layer getLayer(EID e) const;
	Layer getLayerInTree(EID e) const;
	Layer getLayerInTree(const Node *node) const;

	///////////////////////////////////////////////////////
	void setFlag(EID e, Flag flag, bool value);
	void setFlag(Node *node, Flag flag, bool value);
	bool hasFlag(EID e, Flag flag) const;
	bool hasFlag(const Node *node, Flag flag) const;
	bool hasFlagInTreeAnd(EID e, Flag flag) const;
	bool hasFlagInTreeOr(EID e, Flag flag) const;
	bool hasFlagInTreeAnd(const Node *node, Flag flag) const;
	bool hasFlagInTreeOr(const Node *node, Flag flag) const;

	///////////////////////////////////////////////////////

	/// 新しいタグを登録する
	/// ただし、タグ 0 は登録できない
	void defineTag(int tag, const Path &name);

	/// タグが登録されているか調べる。
	bool isTagDefined(int tag) const;

	int getTagByName(const Path &name);

	/// エンティティ e にタグを設定する。
	/// タグはひとつのエンティティにひとつだけ設定できる
	void setTag(EID e, int tag);

	/// エンティティのタグを得る。
	/// エンティティが存在しない場合は 0 を返す
	int getTag(EID e) const;
	int getTagInTree(EID e) const;
	int getTagInTree(const Node *node) const;
	bool hasTag(EID e, int tag) const;
	bool hasTagInTree(EID e, int tag) const;
	bool hasTagInTree(const Node *node, int tag) const;
	
	///////////////////////////////////////////////////////

	/// ツリー全体を深さ優先で巡回したときの順序値を得る
	/// order: 通し番号。ツリー全体を深さ優先で番号を割り当てたときの値。
	/// first: このノードに最初に訪れたときの順序値。行きと帰りの両方をカウントするため、ノード総数よりも大きな数値になる場合がある
	/// second: 子を巡回し終わり、このノードに戻ってきたときの順序値。子を持たない場合は first と同じ値になる
	void getTraversalIndex(EID e, int *order, int *first, int *second) const;
	void getTraversalIndex(const Node *node, int *order, int *first, int *second) const;
	void dirtyTraversalIndex();

	///////////////////////////////////////////////////////

	/// ルートの数を返す
	int getRootCount() const;

	/// ルートを返す
	EID getRoot(int index) const;

	/// ルートインデックスを返す。見つからない場合は -1 を返す
	int getRootIndex(EID root) const;
	int getRootIndex(const Path &name) const;

	/// ルートエンティティの順番を変更する。index_from から要素を削除し、index_to に挿入する。
	/// index_to に getRootCount() 以上の値を指定すると末尾への追加になる
	void moveRoot(int index_from, int index_to);

	
	///////////////////////////////////////////////////////

	/// 子要素の数を返す。孫は含まない
	int getChildCount(EID e) const;

	/// 子要素を返す
	EID getChild(EID e, int index) const;

	/// 子のインデックスを返す。見つからない場合は -1 を返す
	int getChildIndex(EID e, EID child) const;
	int getChildIndex(EID e, const Path &name) const;
	int getChildIndex(const Node *parent, const Node *child) const;

	void moveChild(EID e, int index_from, int index_to);
	void moveChild(Node *node, int index_from, int index_to);
	void moveToFront(EID e);
	void moveToBack(EID e);

	///////////////////////////////////////////////////////
	/// 親要素を返す
	EID getParent(EID e) const;

	/// 親を変更する
	/// parent に NULL を指定した場合、child はルート要素になる
	void setParent(EID e, EID parent);


	///////////////////////////////////////////////////////
	/// エンティティの有効・無効状態を設定する
	/// 有効にした場合、描画と更新が有効になる
	void setEnabled(EID e, bool val);
	bool isEnabled(EID e) const;
	bool isEnabledInTree(EID e) const;

	/// エンティティの一時停止状態を設定・解除する
	///一時停止状態にした場合、そのエンティティは現在の状態を保ち、新しい状態で上書きされなくなる（＝停止する）
	void setPaused(EID e, bool val);
	bool isPaused(EID e) const;
	bool isPausedInTree(EID e) const;

	/// エンティティを表示・非表示にする
	/// 非表示にした場合、そのエンティティは描画されなくなる
	void setVisible(EID e, bool val);
	bool isVisible(EID e) const;
	bool isVisibleInTree(EID e) const;
	
	void removeTree(EID e, bool remove_self=true);

	void sortChildren(EID e, IChildrenSoatingPred &pred);

	/// e が descendant の親または祖先であれば true を返す
	bool isAncestorOf(EID e, EID descendabt) const;

	/// e を起点に、その子要素を深さ優先で巡回しながら walker->onVisit() を呼び出す（起点を除く）。
	/// e に NULL を指定した場合は、すべてのルート要素に対して深さ優先検索を実行する
	void scanChildren(EID e, IHierarchyWalker &walker, int tag=0);

	/// e を起点に、親をたどりながら walker->onVisit() を呼び出す（起点を除く）。
	void scanParents(EID e, IHierarchyWalker &waler);

	int getSerialIndex(EID e);

	////////////////////////////
	int numAllEntities() const;
	void getAllEntities(EntityList &list) const;

	bool exists(EID e) const;

	Node * _getTreeNode(EID e);
	const Node * _getTreeNode(EID e) const;

	////////////////////////////

	void copyTransform(EID dst, EID src);

	/// 変形行列を得る
	/// 適用順序は 平行移動→回転→拡大の順番になる (TRS-Matrix)
	void getMatrix(EID e, Matrix4 *out);
	/// 変形行列の逆行列を得る
	void getMatrixInversed(EID e, Matrix4 *out);
	bool getLocalToWorldMatrix(EID e, Matrix4 *out) const;
	bool getWorldToLocalMatrix(EID e, Matrix4 *out) const;
	Vec3 localToWorldPoint(EID e, const Vec3 &local) const;
	Vec3 worldToLocalPoint(EID e, const Vec3 &world) const;

	const Vec3 & getPosition(EID e);
	Vec3 getPositionInWorld(EID e);
	const Vec3 & getScale(EID e);
	const Vec3 & getPivot(EID e);
	const Quat & getRotation(EID e);
	void setPosition(EID e, const Vec3 &value);
	void setPosition(EID e, float x, float y, float z=0.0f);
	void setScale(EID e, const Vec3 &value);
	void setScale(EID e, float sx, float sy, float sz=1.0f);

	/// 回転を設定する
	void setRotation(EID e, const Quat &quaternion);
	void setRotationIdentity(EID e);
	void setRotation(EID e, const Vec3 &axis, float degrees);
	void setRotationZ(EID e, float degrees);

	/// 軸 axis 周りに delta_degrees だけ回転を加える
	void rotate(EID e, const Vec3 &axis, float degrees);

	/// Z軸回りに degrees だけ回転を加える
	void rotateZ(EID e, float degrees);
	  
	void setPivot(EID e, const Vec3 &value);
	bool isIndependent(EID e);
	void setIndependent(EID e, bool value);
	void dirtyAllNodes();

private:
	/// 子要素を追加する
	/// parent に NULL を指定した場合、child はルート要素になる
	void _setParent(Node *node, Node *parent);
	
	/// 子要素を削除する
	void _removeChildAt(Node *node, int index);
	void _traverse(Node *node) const;

	/// 子要素を深さ優先検索し、すべての要素（ただし起点要素を除く）に対して walker->onVisit() を呼び出す
	/// tag が指定されていれば、そのタグでフィルタリングする。tag が 0 ならば全てのエンティティが対象になる
	void _scan(Node *node, IHierarchyWalker &walker, int tag);

	std::vector<Node *> roots_;
	std::unordered_map<EID, Node> nodes_;
	mutable std::vector<Node *> ordered_nodes_;
	mutable int tmp_traversal_index_;
	mutable int tmp_traversal_order_;
	mutable bool dirty_traversal_index_;

	void zeroClear();
	Matrix4 _computeWorldMatrix(const Node *node) const;
	void _computeLocalMatrix(const Node *node, Matrix4 *m, Matrix4 *inv_m) const;
	const Node * _getLinkedParent(const Node *node) const;

	void selfCheck();

	std::unordered_map<int, Path> tags_;
	std::unordered_set<Path> defined_tags_;
	HierarchyInspector *inspector_;
	Engine *engine_;
};

void Test_entity_tree();

}
