#pragma once
#ifndef __ASYNCLOG_HEADER__
#define __ASYNCLOG_HEADER__
// @nolimitk
// 13.09.13
// asynchronous log

#include <string>
#include <atomic>

#include "../NKCore.h"
#include "LogLayout.h"
#include "LogCategory.h"
#include "Log.h"

namespace NKLog
{
	class LogData : public NKCore::TNode<LogData>
	{
	public:
		const LogLayout _log_layout;
		const LogCategory _log_category;
		const NKWString _log;

	public:
		LogData(const LogLayout& layout, const LogCategory& category, const NKWString& log)
			:_log_layout(layout)
			, _log_category(category)
			, _log(log)
		{
		}
		virtual ~LogData(void) {}
	};

	// log thread must be independent system.
	class LogThread : public NKCore::NKThread
	{
	public:
		virtual bool onRun(void) override;

	protected:
		static const std::chrono::milliseconds DEFAULT_GAP_LOGWRITE;

	public:
		LogThread(void)
		{
		}
		virtual ~LogThread(void) {}

		friend class AsyncLog;
	};
		
	class AsyncLog : public Log
	{
	public:
		LogData* getLogdataQueue(void);

		// @thread-safe, @lock-free
	public:
		bool write(const LogLayout& layout, const LogCategory& category, const NKWString& log);
		///

		void close(void);
		
	protected:
		bool write(LogData* pLogData);
		bool flush(void);
		
	protected:
		NKCore::TLockFreeQueue<LogData> _logdata_queue;

		LogThread _log_thread;
		std::atomic<bool> _start_thread;
		
	public:
		AsyncLog(void);
		virtual ~AsyncLog(void);
	};

	using AsyncLogSingleton = NKCore::Singleton<AsyncLog>;
}

#endif // __ASYNCLOG_HEADER__