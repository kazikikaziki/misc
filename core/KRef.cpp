#include "KRef.h"
//
#include "__Config.h" // MO_STRICT_CHECK
#include "KLog.h"
#include <unordered_set>
#include <mutex>

// 参照カウンタの整合性および解放ミスをチェックするかどうか。
// （GLUT を使う場合はチェックしてはいけない。GLUTは終了処理ができないので、チェックすると必ずエラーになる）
#define CHECK_REFCNT   MO_STRICT_CHECK



namespace mo {


class CReferenceCountedObjectTable {
	std::unordered_set<KRef *> locked_objects_;
public:
	CReferenceCountedObjectTable() {
	}
	~CReferenceCountedObjectTable() {
		if (CHECK_REFCNT) {
			notiyErrorIfNotEmpty();
		}
	}
	void notiyErrorIfNotEmpty() {
		if (locked_objects_.empty()) return;
		for (auto it=locked_objects_.begin(); it!=locked_objects_.end(); ++it) {
			KRef *ref = *it;
			// ログ出力中に参照カウンタが操作されないよう、Logging は使わない
			Log_text(typeid(*ref).name());
		}
		// ここに到達してブレークした場合、参照カウンタがまだ残っているオブジェクトがある。
		// デバッガーで locked_objects_ の中身をチェックすること。
		locked_objects_; // <-- 中身をチェック
		Log_assert(0);
	}
	void addObject(KRef *ref) {
		Log_assert(ref);
		locked_objects_.insert(ref);
	}
	void delObject(KRef *ref) {
		Log_assert(ref);
		locked_objects_.erase(ref);
	}
};

static CReferenceCountedObjectTable s_refcnt_objects;
static std::mutex s_refcnt_mutex;

KRef::KRef() {
	refcnt_ = 1;
	if (CHECK_REFCNT) {
		s_refcnt_mutex.lock();
		s_refcnt_objects.addObject(this);
		s_refcnt_mutex.unlock();
	}
}
KRef::~KRef() {
	Log_assert(refcnt_ == 0); // ここで引っかかった場合、 drop しないで直接 delete してしまっている
	if (CHECK_REFCNT) {
		s_refcnt_mutex.lock();
		s_refcnt_objects.delObject(this);
		s_refcnt_mutex.unlock();
	}
}
void KRef::grab() const {
	s_refcnt_mutex.lock();
	refcnt_++;
	s_refcnt_mutex.unlock();
}
void KRef::drop() const {
	s_refcnt_mutex.lock();
	refcnt_--;
	s_refcnt_mutex.unlock();
	Log_assert(refcnt_ >= 0);
	if (refcnt_ == 0) {
		delete this;
	}
}
int KRef::getReferenceCount() const {
	return refcnt_;
}



} // namespace
