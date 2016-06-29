#ifndef __SERIALIZER_HEADER__
#define __SERIALIZER_HEADER__
// @nolimitk
// 13.09.06
// serializer

#include "EventSlot.h"
#include "RealTimeJob.h"
//#include "SerializeContainer.h"

namespace NKScheduler
{
	class Serializer : public EventSlot
	{
	public:
		bool Execute(int64 executeIndex);

	public:
		bool AddTimeJob(RealTimeJob *pJob, uint tick);
		bool DeleteTimeJob(RealTimeJob *pJob);
		bool ResetTimeJob(RealTimeJob *pJob, uint tick);

	public:
		bool RegisterScheduler(uint tick);

	public:
		virtual bool Process(NKNetwork::EventContext *pEventContext, DWORD transferred);

	public:
		static const int RECOMMAND_SHORTTERMJOB_SIZE = 41;
		static const int SCHEDULER_TIME_UNIT = 50;

	protected:
		// @TODO lock�� ���� �ʴ� ����� ������ ����.
		// @TODO Serializer�� �ϳ��� thread������ ������ �ȴٰ� ������ �Ǹ� lock�� ���� �ʾƵ� �ȴ�.
		// @TODO Serializer�� job�� �߰��ϴ� thread�� �ϳ��� �Ǿ�� �Ѵ�.
		SerializeContainer _container;
		volatile uint _lastReservedTick;
		uint64 _lastExecutionIndex;

	public:
		Serializer(void);
		virtual ~Serializer(void);
	};
}

#endif // __SERIALIZER_HEADER__