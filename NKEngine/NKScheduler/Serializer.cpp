#include "Serializer.h"
#include <crtdbg.h>
#include "../NKEngineLog.h"
//#include "Scheduler.h"
//#include "NKCore/NKClock.h"

using namespace NKScheduler;

Serializer::Serializer(void)
	:_lastReservedTick(0)
	, _container(RECOMMAND_SHORTTERMJOB_SIZE)
{
}

Serializer::~Serializer(void)
{
	_container.Destroy();
}

bool Serializer::Execute(int64 executeIndex)
{
#if defined _SCHEDULER_DEBUG_LOG_
	//NE_DEBUGLOG( L"serializer executed, %I64u", executeIndex );
#endif

	// @TODO job ó�� �ð��� �����Ͽ� monitoring �Ѵ�.
	NKCore::NKClock clock;
	int64 executeTime = 0;

	RealTimeJob *pJob = NULL;
	RealTimeJob *pNextJob = NULL;

	_lastReservedTick = (uint)(-1);
	
	for( int64 i = _lastExecutionIndex ; i <= executeIndex ; i++ )
	{
		// pop queue
		pJob = _container.PopQueue(i);
		//

		if( pJob != NULL && i != executeIndex )
		{
			if( i <= (executeIndex - RECOMMAND_SHORTTERMJOB_SIZE) )
			{
				// @nolimitk container�� short term slot�� ũ�⺸�� �����Ǹ� ���װ� �߻��Ѵ�.
				NKENGINELOG_ERROR( L"serializer executed delayed, it can be bug, current %I64d, org %I64d, last %I64d, execute %I64d",
					i, pJob->getExecuteSlotIndex(), _lastExecutionIndex, executeIndex  );
			}
			NKENGINELOG_WARNING( L"serializer executed delayed, current %I64d, org %I64d, last %I64d, execute %I64d",
				i, pJob->getExecuteSlotIndex(), _lastExecutionIndex, executeIndex  );
		}
	
		// execution
		while( pJob != NULL )
		{
			// @nolimitk job�� �����߿� �ٽ� ����� �� �ֵ��� �ʱ�ȭ�� ���� �Ѵ�.
			pNextJob = pJob->getNext();
#if defined _SCHEDULER_DEBUG_LOG_
			//NE_DEBUGLOG( L"job executed, job %I64u, last %I64u, execute %I64u", pJob->getExecuteSlotIndex(), _lastExecutionIndex, executeIndex  );
#endif
			pJob->UnRegister();
			pJob->ReleaseReserve();

			pJob->Process( (uint64)executeIndex );
			pJob->Unref();
		
			pJob = pNextJob;
		}
		//
	}

	_lastExecutionIndex = executeIndex;

	// instant execution
	pJob = _container.PopInstantQueue();
	while( pJob != NULL )
	{
		while( pJob != NULL )
		{
			// instant execution�� �ٷ� ����ǰų� 1tick�� ������ ����� �� �ִ�.
			if( pJob->getExecuteSlotIndex() != (uint)(executeIndex-1) &&
				pJob->getExecuteSlotIndex() != (uint)executeIndex )
			{
				NKENGINELOG_WARNING( L"%I64u, job instant execution delayed, slot %I64u", executeIndex, pJob->getExecuteSlotIndex() );
			}

			pNextJob = pJob->getNext();
			pJob->UnRegister();
			pJob->ReleaseReserve();
			
			pJob->Process( (uint64)executeIndex );
			pJob->Unref();
				
			pJob = pNextJob;
		}

		// instant execution�� ������ �߻��� �� �����Ƿ� �ð� ������ �д�.
		executeTime = clock.getElapsedMicroSec();
		if( executeTime > SCHEDULER_TIME_UNIT )
		{
			NKENGINELOG_WARNING( L"%I64u, instant execution limited, execution %td", executeIndex, executeTime );
			break;
		}

		// instant execution ���߿� instant execution��û�� �ٽ� �� �� �ִ�.
		pJob = _container.PopInstantQueue();
	}
	//
	
	// @nolimitk ���� ������ ���� ���� tick�� ã�´�.
	// 14.06.09 �����߿� ������� ��쿡�� �̹� ��ϵ� �� ���� �����ؾ��� job�� ���� �� �ִ�.
	// ���� ���� ���ɼ��� �����Ѵ�.
	pJob = _container.PickQueue( executeIndex );
	if( pJob != NULL )
	{
		// 14.06.09 �����߿� ������� ��쿡�� �̹� ��ϵ� �� ���� �����ؾ��� job�� ���� �� �ִ�.
		if( _executeSlotIndex == 0 || pJob->getExecuteSlotIndex() < _executeSlotIndex )
		{
			int rotateIndex = (int)(executeIndex % RECOMMAND_SHORTTERMJOB_SIZE);
			int tick = 0;

			int slotType = pJob->getSlotType();
			if( slotType == 1 )
			{
				// short term slot
				int slotIndex = pJob->getSlotIndex();

				if( slotIndex == rotateIndex ) tick = RECOMMAND_SHORTTERMJOB_SIZE*SCHEDULER_TIME_UNIT;
				else if( slotIndex > rotateIndex ) tick = (int)(slotIndex - rotateIndex) * SCHEDULER_TIME_UNIT;
				else if( slotIndex < rotateIndex ) tick = (RECOMMAND_SHORTTERMJOB_SIZE - rotateIndex + (int)slotIndex) * SCHEDULER_TIME_UNIT;
			}
			else
			{
				// long term slot
				tick = (int)(pJob->getExecuteSlotIndex()-executeIndex) * SCHEDULER_TIME_UNIT;
			}

			NE_DEBUGLOG( L"%I64u, next serializer, slot %d, %d, execute %I64d, rotate %d, tick %d, serializer execute %I64u",
				executeIndex, pJob->getSlotType(), pJob->getSlotIndex(), pJob->getExecuteSlotIndex(), rotateIndex, tick, _executeSlotIndex );

			RegisterScheduler(tick);
		}
	}

	return true;
}

bool Serializer::AddTimeJob(RealTimeJob *pJob, uint tick)
{
	// @TODO tick�� max���� ���Ѵ�.
	
	// index 50~99 -> 1, 100~149 -> 2, 150->199 -> 3, 200->249 -> 4,,, 950~999 -> 19
	int slice = (int)(tick / SCHEDULER_TIME_UNIT);
	if( _container.Insert( slice, pJob ) == false )
	{
		NKENGINELOG_ERROR( L"serializer addjob error,tick %u, slice %d", tick, slice );
		return false;
	}
	
	pJob->Ref();
	pJob->SetContainer(this);
	pJob->SetTick(tick);

#if defined _SCHEDULER_DEBUG_LOG_
	//NE_DEBUGLOG( L"serializer addjob, tick %u, slice %d, slot %I64u", tick, slice, pJob->getExecuteSlotIndex() );
#endif

	RegisterScheduler(tick);
	return true;
}

bool Serializer::DeleteTimeJob(RealTimeJob *pJob)
{
	NE_DEBUGLOG( L"serializer delete slot, type %d, slot %d, executeslot %I64u", pJob->getSlotType(), pJob->getSlotIndex(), pJob->getExecuteSlotIndex() );

	if( _container.Remove( pJob ) == false )
	{
		NKENGINELOG_ERROR( L"serializer delete slot failed, type %d, slot %d, executeslot %I64u", pJob->getSlotType(), pJob->getSlotIndex(), pJob->getExecuteSlotIndex() );
		return false;
	}

	pJob->SetContainer(nullptr);
	pJob->SetTick(0);
	pJob->Unref();

	// @TODO �ش� slot�� job�� ���� ���� ��ϵ� tick�� ��� ����ؾ� �Ѵ�.
	// @TODO ������� ���� ��� ������ �ø���������� �ѹ� ����� �� �ִ�.

	return true;
}

bool Serializer::ResetTimeJob(RealTimeJob *pJob, uint tick)
{
	// @TODO tick�� max���� ���Ѵ�.
	if( tick == 0 ) tick = 1;
	
	// index 50~99 -> 1, 100~149 -> 2, 150->199 -> 3, 200->249 -> 4
	int slice = (int)(tick / SCHEDULER_TIME_UNIT);

	NE_DEBUGLOG( L"serializer remove and insert, tick %u, want %d, type %d, slot %d, executeslot %I64u",
		tick, slice, pJob->getSlotIndex(), pJob->getSlotType(), pJob->getSlotIndex(), pJob->getExecuteSlotIndex() );

	if( _container.RemoveAndInsert( slice, pJob ) == false )
	{
		NKENGINELOG_ERROR( L"serializer resetjob failed, tick %u, want %d", tick, slice );
		return false;
	}

	if( GetReserved() == false )
	{
		// ����� �ȵǾ� ������ ���״�.
		NKENGINELOG_ERROR( L"serializer resetjob error, tick %u, want %d", tick, slice );
	}
	else
	{
		if( tick < _lastReservedTick )
		{
			_lastReservedTick = tick;
			if( getScheduler()->ResetSlot( this, tick ) == true )
			{
				// execution index to start serializer
				_lastExecutionIndex = _executeSlotIndex;
				return true;
			}

			return false;
		}

		return true;
	}

	return false;
}

bool Serializer::RegisterScheduler(uint tick)
{
	bool result = false;
	if( TryReserve() == true )
	{
		_lastReservedTick = tick;
		result = getScheduler()->AddSlot( this, tick );
	}
	else
	{
		if( tick < _lastReservedTick )
		{
			_lastReservedTick = tick;
			result = getScheduler()->ResetSlot( this, tick );
		}
	}

	if( result == true )
	{		
		// execution index to start serializer
		_lastExecutionIndex = _executeSlotIndex;
	}
	return true;
}

bool Serializer::Process(EventContext *pEventContext, DWORD transferred)
{
	_ASSERT( pEventContext->_type == EVENTCONTEXT_SCHEDULER );
	__SET_THREADSAFE(_lockObjectType);

	SchedulerContext *pContext = (SchedulerContext *)pEventContext;

	Execute(pContext->_param);

	SAFE_DELETE(pEventContext);

	Unref();
	return true;
}