#pragma once
#include <vector>

namespace mo {

/// @internal
struct __EID { int not_used; };

/// エンティティの識別子
///
/// @note
/// エンティティ識別子 EID は構造体 __EID のポインタとして定義されるが、
/// これは型チェックのために __EID という型を導入しているだけで、
/// コンパイル時に型チェックさせる以上の使い道はない。
/// __EID 自体は全く使われていないし、使ってはいけない。
/// そもそも EID が __EID へのポインタを保持しない。
typedef __EID *EID;

/// EID --> int の変換
inline int EID_toint(EID x) { return (int)x; }

/// int --> EID の変換
inline EID EID_from(int x) { return (EID)x; }

typedef std::vector<EID> EntityList;

}
