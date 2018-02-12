// vim: ft=cpp

/*
 * watchdog.h --
 *
 *     Implementation of an instruction-based watchdog
 *
 * (c) 2013 Martin Kriegel <mkriegel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include "app"
#include "redundancy.h"

namespace Romain {

	class Watchdog
	{
		public:

			enum SynchronizationMode
			{
				SingleStepping,
				Breakpointing
			};

			enum
			{
				SynchronizationTimeout = 400
			};

		private:

			enum 
			{ 
				ResetSyscallNr 		= 0x1,
				StartSyscallNr 		= 0x2,
				StopSyscallNr  		= 0x3,
				DisableSyscallNr	= 0x4
			};

			enum ChecksumReplicasRet
			{
				AllStatesEqual, /* everything is fine */
				MajorityStatesEqual, /* can be recovered */
				MinorityStatesEqual,
				AllStatesDiffer /* worst case */
			};

			bool 									_enabled;

			unsigned 							_num_instances;

			SynchronizationMode 	_mode;

			App_thread*						_orig_vcpu[Romain::MAX_REPLICAS];

			Romain::RedundancyCallback *_redundancyCB;

			pthread_cond_t 				_help;
			pthread_mutex_t 			_help_mtx;
			unsigned 							_help_count;

			pthread_cond_t 				_leave;
			pthread_mutex_t 			_leave_mtx;
			unsigned 							_leave_count;

			pthread_mutex_t 			_bp_mtx;
	

			l4_cpu_time_t tsc;
			unsigned _trap_count;
	
			/*
 			 * Watchdog: determine if we need to retry synchronizing the replicas
 			 */ 		
			bool 									_retry;
			unsigned 							_retry_count;
	
			/*
 			 * Watchdog: determine if we are shortly before a syscall
 			 */
			bool 									_skip;
	
			/*
 			 * Watchdog: determine if we was passed by another trap while sync
 			 */ 
			bool 									_passed;

			/*
 			 * Watchdog: we need to remember the current leader and if retrying
 			 * 					 the old leader
 			 */ 
			Romain::App_thread 		*_leader, *_old_leader;

			ChecksumReplicasRet checksum_replicas();

			void post_verification(Romain::App_model *a);

			Romain::RedundancyCallback::EnterReturnVal leave(Romain::App_instance *i,
																											 Romain::App_thread *t,
																											 Romain::App_model *a);
		public:

			Watchdog(unsigned instances, bool enable, SynchronizationMode mode = Breakpointing)
				: _num_instances(instances), _enabled(enable), _mode(mode),
					_help_count(0), _leave_count(0), _retry_count(0)
			{
				_check(pthread_mutex_init(&_help_mtx, NULL) != 0, "error initializing mtx");
				_check(pthread_cond_init(&_help, NULL) != 0, "error initializing condvar");
				_check(pthread_mutex_init(&_leave_mtx, NULL) != 0, "error initializing mtx");
				_check(pthread_cond_init(&_leave, NULL) != 0, "error initializing condvar");
				_check(pthread_mutex_init(&_bp_mtx, NULL) != 0, "error initializing mtx");	
			}

			bool enabled() { return _enabled; }
			bool enable(App_instance *i, App_thread *t);

			bool disable(App_instance *i, App_thread *t);		
			bool disable_all();

			bool reset(App_instance *i, App_thread *t, l4_umword_t timeout);

			bool start(App_instance *i, App_thread *t);
			bool stop(App_instance *i, App_thread *t);

			void switch_mode(SynchronizationMode m) { _mode = m; }

			void set_redundancy_callback(Romain::RedundancyCallback *r) { _redundancyCB = r; }

			void skip(bool s) { _skip = s; }
			bool skip() { return _skip; }

			void passed(bool p) { _passed = p; }
			bool passed() { return _passed; }

			void retry(bool r) { _retry = r; }
			void retry_count(unsigned c) { _retry_count = c; }
			unsigned retry_count() { return _retry_count; }

			void leader(App_thread *l) { _leader = l; }
			void old_leader(App_thread *o) { _old_leader = o; }	
			
			Romain::RedundancyCallback::EnterReturnVal enter(Romain::App_instance *i,
																										 	 Romain::App_thread *t,
																										 	 Romain::App_model *a);
			
			Romain::RedundancyCallback::EnterReturnVal breakpointing(Romain::App_instance *i,
																															 Romain::App_thread *t,
																															 Romain::App_model *a);

			Romain::RedundancyCallback::EnterReturnVal single_stepping(Romain::App_instance *i,
																																 Romain::App_thread *t,
																																 Romain::App_model *a);


	};
}

