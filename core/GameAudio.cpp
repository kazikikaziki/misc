#include "GameAudio.h"
//
#include "Engine.h"
#include "KEasing.h"
#include "KFileLoader.h"
#include "KImgui.h"
#include "KLog.h"

#if 1
#	define audio_trace Log_trace
#else
#	define audio_trace Log_print_trace(__FILE__, __LINE__, __FUNCTION__)
#endif

//
// http://forum.devmaster.net/t/openal-lesson-8-oggvorbis-streaming-using-the-source-queue/2895
//
namespace mo {

AudioSystem::AudioSystem() {
	master_volume_ = 1.0f;
	master_mute_ = false;
	solo_group_ = -1;
	AudioGroup def_group;
	def_group.name = "Default";
	groups_.push_back(def_group);
	engine_ = NULL;
}
AudioSystem::~AudioSystem() {
	onEnd();
}
void AudioSystem::init(Engine *engine) {
	audio_trace;
	engine->addSystem(this);
	engine_ = engine;
}
void AudioSystem::onEnd() {
	clearAllSounds();
}
int AudioSystem::getGroupCount() {
	return (int)groups_.size();
}
void AudioSystem::setGroupCount(int count) {
	if (count > 0) {
		groups_.resize(count);
	} else {
		groups_.clear();
	}
}
float AudioSystem::getGroupMasterVolume(int group_id) {
	if (0 <= group_id && group_id < (int)groups_.size()) {
		return groups_[group_id].master_volume;
	}
	return 0.0f;
}
float AudioSystem::getGroupVolume(int group_id) {
	if (0 <= group_id && group_id < (int)groups_.size()) {
		return groups_[group_id].volume;
	}
	return 0.0f;
}
void AudioSystem::setGroupMasterVolume(int group_id, float volume) {
	if (0 <= group_id && group_id < (int)groups_.size()) {
		groups_[group_id].master_volume = volume;
		updateSoundVolumes(group_id);
	}
}
void AudioSystem::stopFadeAll() {
	for (auto it=fades_.begin(); it!=fades_.end(); ++it) {
		it->finished = true;
	}
}
void AudioSystem::stopFade(SOUNDID id) {
	for (auto it=fades_.begin(); it!=fades_.end(); ++it) {
		if (it->sound_id == id) {
			it->finished = true;
		}
	}
}
void AudioSystem::stopFadeByGroup(int group_id) {
	for (auto it=fades_.begin(); it!=fades_.end(); ++it) {
		if (it->group_id == group_id) {
			it->finished = true;
		}
	}
}
void AudioSystem::set_group_volume(int group_id, float volume) {
	groups_[group_id].volume = volume;
	updateSoundVolumes(group_id);
}

void AudioSystem::setGroupVolume(int group_id, float volume, int time) {
	if (0 <= group_id && group_id < (int)groups_.size()) {
		// 進行中のフェードを停止する
		stopFadeByGroup(group_id);

		if (time > 0) {
			Fade fade;
			fade.duration = time;
			fade.time = 0;
			fade.sound_id = NULL;
			fade.group_id = group_id;
			fade.volume_start = getGroupVolume(group_id);
			fade.volume_end = volume;
			fade.auto_stop = false;
			fade.finished = false;
			fades_.push_back(fade);

		} else {
			set_group_volume(group_id, volume);
		}
	}
}
AudioGroupFlags AudioSystem::getGroupFlags(int group_id) {
	if (0 <= group_id && group_id < (int)groups_.size()) {
		return groups_[group_id].flags;
	}
	return 0;
}
void AudioSystem::setGroupFlags(int group_id, AudioGroupFlags flags) {
	if (0 <= group_id && group_id < (int)groups_.size()) {
		// 排他的フラグが設定されている場合は、他のグループの該当フラグを解除する
		if (flags & AudioGroupFlag_Solo) {
			for (auto it=groups_.begin(); it!=groups_.end(); ++it) {
				it->flags &= ~AudioGroupFlag_Solo;
			}
		}
		groups_[group_id].flags = flags;
		updateSoundVolumes(group_id);
	}
}
float AudioSystem::getActualGroupVolume(int group_id) {
	// マスターボリュームがミュート状態の時は無条件でミュート
	if (master_mute_) return 0.0f;

	// SOLO属性がある場合、SOLO以外の音は全てミュート
	if (solo_group_ >= 0 && group_id != solo_group_) return 0.0f;

	// グループタインでミュートが設定されていればミュート
	const AudioGroup &group = groups_[group_id];
	if (group.flags & AudioGroupFlag_Mute) return 0.0f;

	// 音量 = [マスターボリューム] * [グループ単位でのマスターボリューム(OptoinなどでBGM, SEごとに設定された音量] * [一時的なグループボリューム（フェードや演出のために一時的に音量を下げるなど）]
	return master_volume_ * group.master_volume * group.volume;
}
const Path & AudioSystem::getGroupName(int group_id) {
	if (0 <= group_id && group_id < (int)groups_.size()) {
		return groups_[group_id].name;
	}
	return Path::Empty;
}
void AudioSystem::setGroupName(int group_id, const Path &name) {
	if (0 <= group_id && group_id < (int)groups_.size()) {
		groups_[group_id].name = name;
	}
}

int AudioSystem::getNumberOfPlaying() const {
	return (int)sounds_.size();
}
int AudioSystem::getNumberOfPlayingInGroup(int group_id) const {
	int num = 0;
	for (auto it=sounds_.cbegin(); it!=sounds_.cend(); ++it) {
		if (it->second.group_id == group_id) {
			num++;
		}
	}
	return num;
}

void AudioSystem::stopAll(int fade) {
	for (auto it=sounds_.cbegin(); it!=sounds_.cend(); ++it) {
		stop(it->first, fade);
	}
}
void AudioSystem::stopByGroup(int group_id, int fade) {
	for (auto it=sounds_.cbegin(); it!=sounds_.cend(); ++it) {
		if (it->second.group_id == group_id) {
			stop(it->first, fade);
		}
	}
}
SOUNDID AudioSystem::playStreaming(const Path &asset_path, bool looping, int group_id) {
	if (master_mute_) return 0;

	FileInput file = engine_->getFileSystem()->getInputFile(asset_path);
	if (file.empty()) {
		Log_errorf("Failed to open asset file: %s", asset_path.u8());
		return 0;
	}
	SOUNDID snd_id = Sound::playStreamingSound(asset_path, file, 0.0f, looping);
	if (snd_id == 0) {
		Log_errorf("No sound named: %s", asset_path.u8());
		return 0;
	}
	Log_verbosef("BGM: %s", asset_path.u8());
	float group_vol = getActualGroupVolume(group_id);
	Snd snd;
	snd.volume = 1.0f;
	snd.group_id = group_id;
	snd.destroy_on_stop = false;
	Sound::setVolume(snd_id, group_vol);
	sounds_[snd_id] = snd;
	return snd_id;
}
SOUNDID AudioSystem::playOneShot(const Path &asset_path, int group_id) {
	if (master_mute_) return 0;
	if (! Sound::isPooled(asset_path)) {
		FileInput file = engine_->getFileSystem()->getInputFile(asset_path);
		if (file.empty()) {
			Log_errorf("Failed to open file: %s", asset_path.u8());
			return 0;
		}
		Sound::poolSound(asset_path, file);
	}
	SOUNDID snd_id = Sound::playPooledSound(asset_path);
	if (snd_id == 0) {
		Log_errorf("No sound named: %s", asset_path.u8());
		return 0;
	}
	Log_verbosef("SE: %s", asset_path.u8());
	float group_vol = getActualGroupVolume(group_id);
	Snd snd;
	snd.volume = 1.0f;
	snd.group_id = group_id;
	snd.destroy_on_stop = true; // 停止したら自動削除
	Sound::setVolume(snd_id, group_vol);
	sounds_[snd_id] = snd;
	return snd_id;
}
void AudioSystem::postDeleteHandle(SOUNDID id) {
	post_destroy_sounds_.insert(id);
}
bool AudioSystem::isValidSound(SOUNDID id) {
	return sounds_.find(id) != sounds_.end();
}
void AudioSystem::clearAllSounds() {
	for (auto it=sounds_.begin(); it!=sounds_.end(); ++it) {
		SOUNDID id = it->first;
		Sound::deleteHandle(id);
	}
	sounds_.clear();
	post_destroy_sounds_.clear();
}
float AudioSystem::getPositionInSeconds(SOUNDID id) {
	return Sound::getPositionTime(id);
}
float AudioSystem::getLengthInSeconds(SOUNDID id) {
	return Sound::getLengthTime(id);
}
void AudioSystem::setLooping(SOUNDID id, bool value) {
	Sound::setLooping(id, value);
}
void AudioSystem::setVolume(SOUNDID id, float value) {
	auto it = sounds_.find(id);
	if (it != sounds_.end()) {
		const SOUNDID s = it->first;
		Snd &snd = it->second;
		snd.volume = value;
		float actual_volume = getActualGroupVolume(snd.group_id) * snd.volume;
		Sound::setVolume(s, actual_volume);
	}
}
void AudioSystem::setPitch(SOUNDID id, float value) {
	Sound::setPitch(id, value);
}
void AudioSystem::stop(SOUNDID id, int time) {
	if (time > 0) {
		audio_trace;
		Fade fade;
		fade.duration = time;
		fade.time = 0;
		fade.sound_id = id;
		fade.group_id = -1;
		fade.volume_start = Sound::getVolume(id);
		fade.volume_end = 0.0f;
		fade.auto_stop = true;
		fade.finished = false;
		fades_.push_back(fade);
	} else {
		audio_trace;
		Sound::pause(id);
		postDeleteHandle(id);
	}
}
bool AudioSystem::isPlaying(SOUNDID id) {
	return Sound::isPlaying(id);
}
void AudioSystem::updateSoundVolumes(int group_id) {
	for (auto it=sounds_.begin(); it!=sounds_.end(); ++it) {
		const SOUNDID id = it->first;
		const Snd snd = it->second;
		if (group_id < 0 || snd.group_id == group_id) {
			float vol = getActualGroupVolume(snd.group_id) * snd.volume;
			Sound::setVolume(id, vol);
		}
	}
}

void AudioSystem::groupCommand(int group_id, const char *cmd) {
	for (auto it=sounds_.begin(); it!=sounds_.end(); ++it) {
		const SOUNDID id = it->first;
		const Snd snd = it->second;
		if (group_id < 0 || snd.group_id == group_id) {
			Sound::command(cmd, (int*)&id);
		}
	}
}
void AudioSystem::setMute(bool value) {
	master_mute_ = value;
}
bool AudioSystem::isMute() const {
	return master_mute_;
}
float AudioSystem::getMasterVolume() {
	return master_volume_;
}
void AudioSystem::setMasterVolume(float volume) {
	master_volume_ = volume;
	updateSoundVolumes(-1);
}
void AudioSystem::onFrameUpdate() {
	// サウンド処理
	for (auto it=fades_.begin(); it!=fades_.end(); ++it) {
		audio_trace;
		Fade *fade = &(*it);
		if (!fade->finished) {
			if (fade->time < fade->duration) {
				float t = fade->normalized_time();
				float v = KEasing::linear(t, fade->volume_start, fade->volume_end);

				if (fade->sound_id) {
					Sound::setVolume(fade->sound_id, v);
				} else if (fade->group_id >= 0) {
					set_group_volume(fade->group_id, v);
				}
				fade->time++;
			} else {
				if (fade->sound_id) {
					if (fade->auto_stop) {
						stop(fade->sound_id, 0);
					}
				}
				fade->finished = true;
			}
		}
		audio_trace;
	}
	if (!fades_.empty()) {
		audio_trace;
		for (auto it=fades_.begin(); it!=fades_.end();/* ++it*/) {
			if (it->finished) {
				it = fades_.erase(it);
			} else {
				++it;
			}
		}
		audio_trace;
	}

	// 再生終了しているサウンドを削除する
	for (auto it=sounds_.begin(); it!=sounds_.end(); ++it) {
		SOUNDID id = it->first;
		const Snd &snd = it->second;
		if (snd.destroy_on_stop) {
			if (! Sound::isPlaying(id)) {
				post_destroy_sounds_.insert(id);
			}
		}
	}

	// 無効化されたサウンドを削除
	if (! post_destroy_sounds_.empty()) {
		audio_trace;
		for (auto it=post_destroy_sounds_.begin(); it!=post_destroy_sounds_.end(); ++it) {
			SOUNDID id = *it;
			Sound::deleteHandle(id);
			sounds_.erase(id);
		}
		post_destroy_sounds_.clear();
		audio_trace;
	}
}
void AudioSystem::onInspectorGui() {
#ifndef NO_IMGUI
	if (ImGui::TreeNodeEx("Groups", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::TreeNodeEx(ImGui_ID(-1), ImGuiTreeNodeFlags_Leaf, "Master")) {
			if (ImGui::SliderFloat("Volume", &master_volume_, 0.0f, 1.0f)) {
				updateSoundVolumes(-1);
			}
			if (ImGui::Checkbox("Mute", &master_mute_)) {
				updateSoundVolumes(-1);
			}
			ImGui::TreePop();
		}
		for (size_t i=0; i<groups_.size(); i++) {
			AudioGroup group = groups_[i];
			if (ImGui::TreeNodeEx(ImGui_ID(i), ImGuiTreeNodeFlags_Leaf, "%s", group.name.u8())) {
				if (ImGui::SliderFloat("Master Volume", &group.master_volume, 0.0f, 1.0f)) {
					setGroupVolume(i, group.master_volume, 0);
				}
				if (ImGui::SliderFloat("Volume", &group.volume, 0.0f, 1.0f)) {
					setGroupVolume(i, group.volume, 0);
				}
				if (ImGui::CheckboxFlags("Mute", (unsigned int *)&group.flags, AudioGroupFlag_Mute)) {
					setGroupFlags(i, group.flags);
				}
				ImGui::SameLine();
				if (ImGui::CheckboxFlags("Solo", (unsigned int *)&group.flags, AudioGroupFlag_Solo)) {
					setGroupFlags(i, group.flags);
				}
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Sounds")) {
		ImGui::Text("Sound count: %d", (int)sounds_.size());
		for (auto it=sounds_.begin(); it!=sounds_.end(); ++it) {
			const SOUNDID sid = it->first;
			const Path &filename = Sound::getSourceName(sid);
			
			float pos_sec = Sound::getPositionTime(sid);
			float len_sec = Sound::getLengthTime(sid);
				
			ImGui::Separator();
			ImGui::Text("Sound: %s", filename.u8());
			ImGui::Text("Pos[sec]: %0.3f/%0.3f", pos_sec, len_sec);

			ImGui::PushID(sid);
			if (ImGui::SliderFloat("Seek", &pos_sec, 0, len_sec)) {
				Sound::setPositionTime(sid, pos_sec);
			}
			float p = Sound::getPitch(sid);
			if (ImGui::SliderFloat("Pitch", &p, 0.1f, 4.0f)) {
				Sound::setPitch(sid, p);
			}
			float vol = Sound::getVolume(sid);
			if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f)) {
				Sound::setVolume(sid, vol);
			}
			float pan = Sound::getPan(sid);
			if (ImGui::SliderFloat("Pan", &pan, -1.0f, 1.0f)) {
				Sound::setPan(sid, pan);
			}
			bool L = Sound::isLooping(sid);
			if (ImGui::Checkbox("Loop", &L)) {
				Sound::setLooping(sid, L);
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
#endif // !NO_IMGUI
}

} // namespace

