#pragma once

namespace mo {


/// 参照カウンタ
class KRef {
public:
	KRef();

	/// 参照カウンタをインクリメントする
	void grab() const;

	/// 参照カウンタをデクリメントする
	/// カウンタがゼロになった場合はデストラクタを呼ぶ
	void drop() const;

	/// 現在の参照カウンタを返す
	int getReferenceCount() const;

protected:
	virtual ~KRef();

private:
	mutable int refcnt_;
};

#define Ref_grab(a)  ((a) ? ((a)->grab(),     NULL) : NULL) // to avoid incompatible operands error
#define Ref_drop(a)  ((a) ? ((a)->drop(), (a)=NULL) : NULL)
#define Ref_copy(dst, src)  { Ref_grab(src); Ref_drop(dst); (dst) = (src); } // src == dst の場合を考慮し、かならず先に grab しておく


} // namespace