#pragma once
#include "system.h"
#include "KSound.h"

namespace mo {

class Engine;

enum AudioGroupFlag {
	AudioGroupFlag_Mute = 1,
	AudioGroupFlag_Solo = 2,
};
typedef int AudioGroupFlags;

class AudioSystem: public System {
public:
	AudioSystem();
	virtual ~AudioSystem();
	void init(Engine *engine);
	virtual void onEnd() override;
	virtual void onFrameUpdate() override;
	virtual void onInspectorGui() override;

	/// 現在のサウンドグループ数を返す
	int getGroupCount();

	/// サウンドグループ数が指定された個数になるように調整する。
	/// 新規追加されたグループの設定はすべてデフォルト値になる
	/// すでに指定された値と同数のグループが存在する場合は何もしない
	void setGroupCount(int count);

	/// グループ単位でのサウンド設定を適用する
	/// 排他的フラグ（同時に１グループにしか適用できないフラグ）を含んでいる場合は、
	/// 自動的に他のグループの該当フラグが解除される
	AudioGroupFlags getGroupFlags(int group_id);

	/// グループ単位でのサウンド設定を得る
	void setGroupFlags(int group_id, AudioGroupFlags flags);

	/// グループ単位でのマスター音量を得る。0.0 - 1.0
	float getGroupMasterVolume(int group_id);

	/// グループ単位での音量を得る。0.0 - 1.0
	float getGroupVolume(int group_id);

	/// グループ単位でのマスター音量を設定する。0.0 - 1.0
	/// このグループに属するすべてのサウンドがこの設定の影響を受ける
	/// 例えばコンフィグ画面で BGM や SE の音量を設定できるようにした時、
	/// ユーザーが指定した音量が setGroupMaterVolume で設定するようにする。
	void setGroupMasterVolume(int group_id, float volume);

	/// グループ単位での音量を設定する 0.0 - 1.0
	/// フェードや、場面によって一時的に音量を下げるなどの操作の場合は
	/// setGroupMaterVolume ではなく setGroupVolume を使う。
	/// 実際のグループ音量は setGroupVolume と setGroupSubVolume の積で決まる
	/// time フェード時間
	void setGroupVolume(int group_id, float volume, int time);

	/// 指定されたグループの実際の音量を返す
	float getActualGroupVolume(int group_id);

	/// サウンドグループ名を得る
	const Path & getGroupName(int group_id);

	/// サウンドグループ名を設定する。これはログやGUIで表示するためのもの
	void setGroupName(int group_id, const Path &name);

	/// マスター音量
	float getMasterVolume();
	void  setMasterVolume(float volume);

	/// 再生中のサウンド数を得る
	int getNumberOfPlaying() const;

	/// 指定されたグループで再生中のサウンド数を得る
	int getNumberOfPlayingInGroup(int group_id) const;

	/// ミュート中かどうか
	bool isMute() const;

	/// すべてのサウンド音量を一時的にゼロにする
	void setMute(bool value);

	/// サウンドファイルをストリーム再生する。
	/// サウンドに割り当てられた識別子を返す
	/// sound サウンドファイル名
	/// looping 連続再生
	/// group_id このサウンドに割り当てるグループ番号
	SOUNDID playStreaming(const Path &sound, bool looping=true, int group_id=0);

	/// サウンドファイルをショット再生する。
	/// サウンドに割り当てられた識別子を返す
	/// sound サウンドファイル名
	/// group_id このサウンドに割り当てるグループ番号
	SOUNDID playOneShot(const Path &sound, int group_id=0);

	/// サウンドを停止する
	void stop(SOUNDID id, int time=0);

	/// 再生中のサウンドをすべて止める
	void stopAll(int fade=0);

	/// 指定されたグループで再生中の全てのサウンドを止める
	void stopByGroup(int group_id, int fade=0);

	/// 進行中のフェード処理を中止する
	void stopFade(SOUNDID id);

	/// 進行中のフェード処理をすべて中止する
	void stopFadeAll();

	/// 指定したグループで進行中のフェード処理をすべて中止する
	void stopFadeByGroup(int group_id);

	/// サウンドを削除する
	void postDeleteHandle(SOUNDID id);

	/// すべてのサウンドを削除する
	void clearAllSounds();

	/// サウンド識別子が利用可能かどうか判定する
	bool isValidSound(SOUNDID id);

	/// 再生位置を秒単位で返す
	/// サウンド識別子が存在しない場合は 0 を返す
	float getPositionInSeconds(SOUNDID id);

	/// サウンド長さを秒単位で返す
	/// サウンド識別子が存在しない場合は 0 を返す
	float getLengthInSeconds(SOUNDID id);

	/// サウンドをリピート再生するかどうかを指定する
	void setLooping(SOUNDID id, bool value);

	/// サウンドごとの音量を指定する（実際の音量はマスター音量との乗算になる→setMasterVolume）
	void setVolume(SOUNDID id, float value);

	/// 再生速度を指定する
	void setPitch(SOUNDID id, float value);

	/// 指定されたサウンドが再生中かどうか調べる
	bool isPlaying(SOUNDID id);

	/// 音量を更新する
	void updateSoundVolumes(int group_id=-1);

	void groupCommand(int group_id, const char *cmd);

private:
	struct Snd {
		float volume;
		int group_id;
		bool destroy_on_stop;
	};
	struct AudioGroup {
		AudioGroup() {
			master_volume = 1.0f;
			volume = 1.0f;
			flags = 0;
			solo = false;
			mute = false;
		}
		Path name;
		AudioGroupFlags flags;
		float master_volume;
		float volume;
		bool solo;
		bool mute;
	};
	struct Fade {
		Fade() {
			sound_id = 0;
			group_id = -1;
			volume_start = 0;
			volume_end = 0;
			duration = 0;
			time = 0;
			auto_stop = false;
			finished = false;
		}
		float normalized_time() const {
			return (float)time / duration;
		}
		SOUNDID sound_id;
		int group_id;
		float volume_start;
		float volume_end;
		int duration;
		int time;
		bool auto_stop;
		bool finished;
	};
	void set_group_volume(int group_id, float volume);
	std::unordered_map<SOUNDID, Snd> sounds_;
	std::unordered_set<SOUNDID> post_destroy_sounds_;
	std::vector<Fade> fades_;
	std::vector<AudioGroup> groups_;
	Engine *engine_;
	int solo_group_; // AudioGroupFlag_Solo が設定されてているグループ番号。未設定ならば -1
	float master_volume_;
	bool master_mute_;
};



} // namespace
