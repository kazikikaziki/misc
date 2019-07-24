#pragma once
#include <thread>
#include "KRef.h"

namespace mo {

class IThread: public virtual KRef {
public:
	IThread();
	virtual ~IThread();
	/// スレッドによって実行される処理
	virtual void onRun() = 0;
	/// スレッドが実行中かどうか
	bool isRunning() const;
	/// スレッドが自然に終了するまで待つ
	void wait();
	/// スレッド中断要求を出し、終了するまで待つ
	void abort();
	/// スレッド実行開始
	void run();

	/// abort() によってスレッド中断要求が出されているなら true を返す
	/// 中断に対応するためには onRun() 内で shouldAbort() の戻り値をチェックすること
	bool shouldAbort() const;

protected:
	static void static_run(IThread *thread);
	static void static_run_no_arg();

private:
	std::thread thread_;
	bool abort_;
	bool running_;
};

typedef int JobId;

enum JobStat {
	JobStat_Invalid,
	JobStat_Working,
	JobStat_Waiting,
	JobStat_Done,
};

class IJob: public virtual KRef {
public:
	virtual void execute() = 0;

	IJob() { id_ = 0; }
	JobId getId() const { return id_; }
	void _setid(JobId id) { id_ = id; } // internal use only

private:
	JobId id_;
};

class IJobThread: public IThread {
public:
	/// ジョブを追加する
	/// 追加したジョブを識別するための値を返す
	/// 追加されたジョブは grab されるため、呼び出し側でこれ以上 job が不要な場合は drop してよい
	virtual JobId addJob(IJob *job) = 0;
	
	/// 未完了のジョブ数（処理中＋待機中）を返す
	virtual int getJobCount() const = 0;
	
	/// ジョブの状態を返す
	virtual JobStat getJobStat(JobId job_id) const = 0;

	/// 指定されたジョブが終了するまで待つ
	virtual void waitJob(JobId job_id) = 0;

	/// 未処理のジョブを削除する
	virtual void clearJobs() = 0;
};

IJobThread * createJobThread();

}