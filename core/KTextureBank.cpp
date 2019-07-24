#include "KTextureBank.h"
#include "KFile.h"
#include "KLog.h"
#include "KNum.h"
#include "KImgui.h"
#include "KPlatform.h"
#include "KStd.h"
#include "KVideo.h"
#include "asset_path.h"
#include <algorithm>

namespace mo {

#define PAL_NAME_SEPARATOR    '&'

TextureBank::TextureBank() {
	//	pow2_texture_only_ = false;
	pow2_texture_only_ = true;
	cb_ = NULL;
}
TextureBank::~TextureBank() {
	clearAssets();
}

int TextureBank::getCount() { 
	return (int)items_.size();
}
PathList TextureBank::getNames() {
	PathList names;
	mutex_.lock();
	{
		names.reserve(items_.size());
		for (auto it=items_.begin(); it!=items_.end(); ++it) {
			names.push_back(it->first);
		}
		std::sort(names.begin(), names.end());
	}
	mutex_.unlock();
	return names;
}
PathList TextureBank::getRenderTexNames() {
	PathList names;
	mutex_.lock();
	{
		for (auto it=items_.begin(); it!=items_.end(); ++it) {
			TexDesc desc;
			Video::getTextureDesc(it->second, &desc);
			if (desc.is_render_target) {
				names.push_back(it->first);
			}
		}
		std::sort(names.begin(), names.end());
	}
	mutex_.unlock();
	return names;
}
void TextureBank::clearAssets() {
	mutex_.lock();
	for (auto it=items_.begin(); it!=items_.end(); ++it) {
		Video::deleteTexture(it->second);
	} 
	items_.clear();
	mutex_.unlock();
}
void TextureBank::delAsset(const Path &name) {
	mutex_.lock();
	auto it = items_.find(name);
	if (it != items_.end()) {
		Video::deleteTexture(it->second);
		mutex_.lock();
		items_.erase(it);
		mutex_.unlock();
		Log_verbosef("Del texture: %s", name.u8());
	}
	mutex_.unlock();
}
bool TextureBank::hasAsset(const Path &name) const {
	mutex_.lock();
	bool ret = items_.find(name) != items_.end();
	mutex_.unlock();
	return ret;
}
Path TextureBank::getName(const TEXID tex) const {
	Path name;
	mutex_.lock();
	for (auto it=items_.begin(); it!=items_.end(); ++it) {
		if (it->second == tex) {
			name = it->first;
			break;
		}
	}
	mutex_.unlock();
	return name;
}
TEXID TextureBank::findTextureRaw(const Path &name, bool should_exist) const {
	TEXID ret = NULL;
	mutex_.lock();
	auto it = items_.find(name);
	if (it != items_.end()) {
		ret = it->second;
	} else if (should_exist) {
		// もしかして
		Path probably;
		for (auto it2=items_.begin(); it2!=items_.end(); it2++) {
			if (it2->first.endsWithPath(name)) {
				probably = it2->first;
				break;
			}
		}
		if (probably.empty()) {
			Log_errorf(u8"E_TEX: テクスチャ '%s' はロードされていません", name.u8());
		} else {
			Log_errorf(u8"E_TEX: テクスチャ '%s' はロードされていません。もしかして: '%s'", name.u8(), probably.u8());
		}
	}
	mutex_.unlock();
	return ret;
}
bool TextureBank::parseName(const Path &name, Path *based_tex, Path *applied_pal) const {
	std::string tex, pal;
	if (K_strsplit_char(name.u8(), &tex, PAL_NAME_SEPARATOR, &pal)) {
		if (based_tex  ) *based_tex = Path::fromUtf8(tex.c_str());
		if (applied_pal) *applied_pal = Path::fromUtf8(pal.c_str());
		return true;
	}
	return false;
}

Path _GetModName(const Path &name, int modifier) {
	return Path::fromFormat("%s%c%d", name.u8(), PAL_NAME_SEPARATOR, modifier);
}
bool _ParseModName(const Path &modname, int *modifier) {
	const char *s = modname.u8();
	const char *p = strchr(modname.u8(), PAL_NAME_SEPARATOR);
	if (p) {
		*modifier = K_strtoi(p + 1);
		return true;
	}
	return false;
}

void TextureBank::clearModifierCaches() {
	PathList texnames;
	mutex_.lock();
	for (auto it=items_.begin(); it!=items_.end(); ++it) {
		const Path &name = it->first;
		int modifier = 0;
		if (_ParseModName(name, &modifier)) {
		//	if (modifier != 0) {
				texnames.push_back(name); // 削除対象
		//	}
		}
	}
	mutex_.unlock();

	for (auto it=texnames.begin(); it!=texnames.end(); ++it) {
		delAsset(*it);
	}
}
TEXID TextureBank::findTexture(const Path &name, int modifier, bool should_exist, EID data) {
	// modifier が指定されていない場合は、普通に探して普通に返す
	if (modifier == 0) {
		return findTextureRaw(name, should_exist);
	}
	// modifierが指定されている。
	// modifier適用済みのテクスチャが生成済みであれば、それを返す
	Path modname = _GetModName(name, modifier);
	{
		TEXID tex = findTextureRaw(modname, false);
		if (tex) {
			return tex;
		}
	}
	// modifier適用済みのテクスチャが存在しない
	// modifierを適用するため、元のテクスチャ画像を得る
	TEXID basetex = findTextureRaw(name, should_exist);
	if (basetex == NULL) {
		return NULL;
	}

	// modifierを適用するための新しいテクスチャを作成する。
	// 念のため、空っぽのテクスチャではなく元画像のコピーで初期化しておく
	TEXID newtex = addTextureFromTexture(modname, basetex);
	if (cb_) {
		cb_->on_videobank_modifier(newtex, name, basetex, modifier, data);
	}
	return newtex;
}
TEXID TextureBank::addTextureFromFile(const Path &name, FileInput &file) {
	TEXID tex = NULL;
#if 1
	tex = findTextureRaw(name, false);
	if (tex) {
		Log_debugf("Texture named '%s' is already exists", name.u8());
		return tex;
	}

#endif
	if (file.isOpen()) {
		std::string bin = file.readBin();
		KImage image;
		if (image.loadFromMemory(bin)) {
			tex = addTextureFromImage(name, image);
		}
	}
	return tex;
}
TEXID TextureBank::addRenderTexture(const Path &name, int w, int h, Flags flags) {
	if (name.empty() || w <= 0 || h <= 0) {
		Log_errorf("E_TEXTUREBANK: Invalid argument at addRenderTexture");
		return NULL;
	}

	if (flags & F_OVERWRITE_ANYWAY) {
		// 同名のテクスチャが存在する場合には無条件で上書きする
		TEXID rentex = Video::createRenderTexture(w, h);
		mutex_.lock();
		if (items_.find(name) != items_.end()) {
			Log_warningf("W_TEXTURE_OVERWRITE: Texture named '%s' already exists. The texture is overritten by new one", name.u8());
			delAsset(name);
		}
		items_[name] = rentex;
		mutex_.unlock();
		return rentex;
	}

	{
		TEXID tex = NULL;

		mutex_.lock();
		auto it = items_.find(name);
		if (it==items_.end() || it->second==NULL) {
			//
			// 同名のテクスチャはまだ存在しない
			//
			tex = Video::createRenderTexture(w, h);
			items_[name] = tex;
			Log_debugf("New render texture: %s (%dx%d)", name.u8(), w, h);

		} else {
			TexDesc desc;
			Video::getTextureDesc(it->second, &desc);
			if (! desc.is_render_target) {
				//
				// 既に同じ名前の非レンダーテクスチャが存在する
				//
				if (flags & F_OVERWRITE_USAGE_NOT_MATCHED) {
					// 古いテクスチャを削除し、新しいレンダーテクスチャを作成して上書きする
					delAsset(name);
					tex = Video::createRenderTexture(w, h);
					items_[name] = tex;
					Log_debugf("Replace texture2D with new render texture: %s (%dx%d)", name.u8(), w, h);
				} else {
					// 使いまわしできない
					Log_errorf("incompatible texture found (type not matched): '%s'", name.u8());
				}

			} else if (desc.w != w || desc.h != h) {
				//
				// 既に同じ名前で異なるサイズのレンダーテクスチャが存在する
				//
				if (flags & F_OVERWRITE_SIZE_NOT_MATCHED) {
					// 古いテクスチャを削除し、新しいレンダーテクスチャを作成して上書きする
					delAsset(name);
					tex = Video::createRenderTexture(w, h);
					items_[name] = tex;
					Log_debugf("Replace render texture with new size: %s (%dx%d)", name.u8(), w, h);
				} else {
					// 使いまわしできない
					Log_errorf("incompatible texture found (size not matched): '%s'", name.u8());
				}

			} else {
				//
				// 既に同じ名前で同じサイズのレンダーテクスチャが存在する
				//
				tex = it->second;
			}
		}
		mutex_.unlock();
		return tex;
	}
}
TEXID TextureBank::addTextureFromSize(const Path &name, int w, int h) {
	if (name.empty()) {
		Log_errorf("E_TEXBANK: Texture name is empty");
		return NULL;
	}
	if (w <= 0 || h <= 0) {
		Log_errorf("E_TEXBANK: Invalid texture size: %dx%d", w, h);
		return NULL;
	}

	TEXID tex = NULL;
	mutex_.lock();
	auto it = items_.find(name);
	if (it!=items_.end() && it->second!=NULL) {
		TEXID T = it->second;
		TexDesc desc;
		Video::getTextureDesc(T, &desc);
		if (desc.is_render_target) {
			// 同じ名前のテクスチャが存在するが、レンダーターゲットとして作成されているために使いまわしできない
			Log_errorf("E_TEXBANK: Incompatible texture found (type not matched): '%s", name.u8());
		} else if (desc.w != w || desc.h != h) {
			// 同じ名前のテクスチャが存在するが、サイズが異なるために使いまわしできない
			Log_errorf("E_TEXBANK: Incompatible texture found (size not matched): '%s'", name.u8());
		} else {
			tex = T;
		}
	} else {
		tex = Video::createTexture2D(w, h);
		items_[name] = tex;
		Log_debugf("New texture: %s (%dx%d)", name.u8(), w, h);
	}
	mutex_.unlock();

	return tex;
}
TEXID TextureBank::addTextureFromImage(const Path &name, const KImage &image) {
	TEXID tex;

	int tex_w, tex_h;
	if (pow2_texture_only_) {
		tex_w = Num_ceil_pow2(image.getWidth());
		tex_h = Num_ceil_pow2(image.getHeight());
	} else {
		tex_w = image.getWidth();
		tex_h = image.getHeight();
	}
	tex = addTextureFromSize(name, tex_w, tex_h);
	if (tex) {
		mo__FancyWriteImage(tex, image);
	}
	return tex;
}
TEXID TextureBank::addTextureFromTexture(const Path &name, TEXID source) {
	KImage img = Video::exportTextureImage(source);
	return addTextureFromImage(name, img);
}

} // namespace
