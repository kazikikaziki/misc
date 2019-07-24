#include "KThread.h"
//
#include <list>
#include <mutex>
#include <assert.h>

#ifdef _WIN32
#	include <Windows.h>
#endif

static void _sleep_about_msec(int msec) {
	// だいたいmsecミリ秒待機する。
	// 精度は問題にせず、適度な時間待機できればそれで良い。
	// エラーによるスリープ中断 (nanosleepの戻り値チェック) も考慮しない
#ifdef _WIN32
	Sleep(msec);
#else
	struct timespec ts;
	ts.tv_sec  = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000 * 1000;
	nanosleep(&ts, &ts);
#endif
}







namespace mo {

#pragma region IThread
#ifdef __APPLE__
// std::thread のコンストラクタに引数付き関数を渡せない時の回避
IThread *s_thread_arg = NULL;
void IThread::static_run_no_arg() {
	s_thread_arg->onRun();
	s_thread_arg->running_ = false;
}
#endif

void IThread::static_run(IThread *thread) {
	thread->onRun();
	thread->running_ = false;
}
IThread::IThread() {
	running_ = false;
	abort_ = false;
}
IThread::~IThread() {
	abort();
}
bool IThread::isRunning() const {
	return running_;
//	return thread_.joinable();
}
void IThread::wait() {
	if (thread_.joinable()) {
		thread_.join();
		running_ = false;
	}
}
void IThread::abort() {
	if (thread_.joinable()) {
		abort_ = true;
		thread_.join();
		running_ = false;
	}
}
bool IThread::shouldAbort() const {
	return abort_;
}
void IThread::run() {
	// スレッドに代入する場合、元のスレッドが動いていてはいけない。
	// スレッド実行中に上書きしようとした場合は std::terminate が発生する。
	// 代入する前に必ず join または detach すること
	// http://cpprefjp.github.io/reference/thread/thread.html
	abort();
	abort_ = false;
	running_ = true;
#ifdef __APPLE__
	s_thread_arg = this;
	std::thread th(static_run_no_arg);
	thread_.swap(th);
#else
	thread_ = std::thread(static_run, this);
#endif
}
#pragma endregion // IThread



class JobThread: public IJobThread {
	JobId uniq_id_;
	std::list<IJob *> jobs_;
	mutable std::mutex mutex_;
public:
	JobThread() {
		uniq_id_ = 0;
		run();
	}
	virtual ~JobThread() {
		wait();
		clearJobs();
	}
	virtual JobId addJob(IJob *job) override {
		assert(job);
		mutex_.lock();
		++uniq_id_;
		job->grab();
		job->_setid(uniq_id_);
		jobs_.push_back(job);
		mutex_.unlock();
		return uniq_id_;
	}
	virtual int getJobCount() const override {
		int cnt;
		mutex_.lock();
		cnt = (int)jobs_.size();
		mutex_.unlock();
		return cnt;
	}
	virtual JobStat getJobStat(JobId job_id) const override {
		if (job_id <= 0 || uniq_id_ < job_id) {
			return JobStat_Invalid;
		}
		JobStat s = JobStat_Done;
		mutex_.lock();
		for (auto it=jobs_.cbegin(); it!=jobs_.cend(); ++it) {
			const IJob *job = *it;
			if (job->getId() == job_id) {
				if (it == jobs_.cbegin()) {
					s = JobStat_Working;
				} else {
					s = JobStat_Waiting;
				}
				break;
			}
		}
		mutex_.unlock();
		return s;
	}
	virtual void waitJob(JobId job_id) override {
		while (getJobStat(job_id) == JobStat_Waiting) {
			// 別のジョブの終了を待っている
			_sleep_about_msec(1);
		}
		while (getJobStat(job_id) == JobStat_Working) {
			// ジョブ処理中である
			_sleep_about_msec(1);
		}
	}
	virtual void clearJobs() override {
		mutex_.lock();
		for (auto it=jobs_.begin(); it!=jobs_.end(); ++it) {
			Ref_drop(*it);
		}
		jobs_.clear();
		mutex_.unlock();
	}
	virtual void onRun() override {
		while (! shouldAbort()) {
			if (getJobCount() == 0) {
				// ジョブなし。待機
				_sleep_about_msec(1);

			} else {
				// 次のジョブを取得
				IJob *job;
				mutex_.lock();
				job = jobs_.front();
				job->grab();
				mutex_.unlock();

				// ジョブ実行
				job->execute();

				// 実行終了したジョブを取り除く
				mutex_.lock();
				jobs_.pop_front();
				job->drop();
				job->drop();
				mutex_.unlock();
			}
		}
	}
};

IJobThread * createJobThread() {
	return new JobThread();
}


} // namespace

