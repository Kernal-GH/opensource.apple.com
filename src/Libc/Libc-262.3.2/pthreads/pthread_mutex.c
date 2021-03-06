/*
 * Copyright 1996 1995 by Open Software Foundation, Inc. 1997 1996 1995 1994 1993 1992 1991
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/*
 * MkLinux
 */

/*
 * POSIX Pthread Library
 * -- Mutex variable support
 */

#include "pthread_internals.h"

/*
 * Destroy a mutex variable.
 */
int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
        if (mutex->sig != _PTHREAD_MUTEX_SIG)
                return (EINVAL);
        if ((mutex->owner != (pthread_t)NULL) ||
            (mutex->busy != (pthread_cond_t *)NULL))
                return (EBUSY);
        mutex->sig = _PTHREAD_NO_SIG;
	return (ESUCCESS);
}

/*
 * Initialize a mutex variable, possibly with additional attributes.
 */
int
pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
        LOCK_INIT(mutex->lock);
        mutex->sig = _PTHREAD_MUTEX_SIG;
        if (attr)
        {
                if (attr->sig != _PTHREAD_MUTEX_ATTR_SIG)
                        return (EINVAL);
                mutex->prioceiling = attr->prioceiling;
                mutex->protocol = attr->protocol;
                mutex->type = attr->type;
		if ((mutex->type == PTHREAD_MUTEX_DEFAULT) || (mutex->type == PTHREAD_MUTEX_NORMAL)) 
			mutex->def = 1;
		else
			mutex->def = 0;
        } else {
                mutex->prioceiling = _PTHREAD_DEFAULT_PRIOCEILING;
                mutex->protocol = _PTHREAD_DEFAULT_PROTOCOL;
                mutex->type = PTHREAD_MUTEX_DEFAULT;
		mutex->def = 1;
        }
        mutex->lock_count = 0;
        mutex->owner = (pthread_t)NULL;
        mutex->next = (pthread_mutex_t *)NULL;
        mutex->prev = (pthread_mutex_t *)NULL;
        mutex->busy = (pthread_cond_t *)NULL;
	mutex->waiters = 0;
	mutex->cond_lock = 0;
	mutex->sem = MACH_PORT_NULL;
	return (ESUCCESS);
}

/*
 * Manage a list of mutex variables owned by a thread
 */
#if defined(DEBUG)
static void
_pthread_mutex_add(pthread_mutex_t *mutex, pthread_t self)
{
        pthread_mutex_t *m;
        if (self != (pthread_t)0) {
            if ((m = self->mutexes) != (pthread_mutex_t *)NULL)
                { /* Add to list */
                	m->prev = mutex;
                }
            mutex->next = m;
            mutex->prev = (pthread_mutex_t *)NULL;
            self->mutexes = mutex;
	}
}

static void
_pthread_mutex_remove(pthread_mutex_t *mutex, pthread_t self)
{
        pthread_mutex_t *n, *prev;
        if ((n = mutex->next) != (pthread_mutex_t *)NULL)
        {
                n->prev = mutex->prev;
        }
        if ((prev = mutex->prev) != (pthread_mutex_t *)NULL)
        {
                prev->next = mutex->next;
        } else
        { /* This is the first in the list */
            if (self != (pthread_t)0) {
                self->mutexes = n;
            }
        }
}
#endif

/*
 * Lock a mutex.
 * TODO: Priority inheritance stuff
 */
int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
        kern_return_t kern_res;
        pthread_t self;
	int slowpath;

        if (mutex->sig == _PTHREAD_MUTEX_SIG_init)
        {
                int res;
                if (res = pthread_mutex_init(mutex, NULL))
                        return (res);
        }
        if (mutex->sig != _PTHREAD_MUTEX_SIG)
                return (EINVAL);        /* Not a mutex variable */

#if !defined(DEBUG)
	if (mutex->def) {
		slowpath = 0;
		self = (pthread_t)0x12141968;
	} else 
#endif /* DEBUG */
	{
		slowpath = 1;
		self = pthread_self();
	}

        LOCK(mutex->lock);
                
	if (mutex->waiters || (mutex->owner != (pthread_t)NULL))
	{
        	if(slowpath && (mutex->owner == self)) {
            		if(mutex->type == PTHREAD_MUTEX_ERRORCHECK ) {
               			UNLOCK(mutex->lock);
               			return(EDEADLK);
            		} else if (mutex->type == PTHREAD_MUTEX_RECURSIVE ) {
				if (mutex->lock_count >= USHRT_MAX){
               				UNLOCK(mutex->lock);
					return(EAGAIN);
				}
                		mutex->lock_count++;
               			UNLOCK(mutex->lock);
               			return(ESUCCESS);
            		}
        	} 
                mutex->waiters++;
                if (mutex->sem == MACH_PORT_NULL) {
			mutex->sem = new_sem_from_pool();
		}
                UNLOCK(mutex->lock);
		do {
			PTHREAD_MACH_CALL(semaphore_wait(mutex->sem), kern_res);
		} while (kern_res == KERN_ABORTED);
                LOCK(mutex->lock);
		mutex->waiters--;
		if (mutex->waiters == 0) {
			restore_sem_to_pool(mutex->sem);
			mutex->sem = MACH_PORT_NULL;
		}
                if (mutex->cond_lock) {
                    mutex->cond_lock = 0;
                }
        }
#if  defined(DEBUG) 
        _pthread_mutex_add(mutex, self);
#endif
        mutex->owner = self;
        if (slowpath && (mutex->type == PTHREAD_MUTEX_RECURSIVE))
                mutex->lock_count = 1;
        UNLOCK(mutex->lock);
        return (ESUCCESS);
}

/*
 * Attempt to lock a mutex, but don't block if this isn't possible.
 */
int
pthread_mutex_trylock(pthread_mutex_t *mutex)
{
        kern_return_t kern_res;
        pthread_t self;
	int slowpath;
	
	if (mutex->sig == _PTHREAD_MUTEX_SIG_init)
        {
                int res;
                if (res = pthread_mutex_init(mutex, NULL))
                        return (res);
        }
        if (mutex->sig != _PTHREAD_MUTEX_SIG)
                return (EINVAL);        /* Not a mutex variable */
        
#if !defined(DEBUG)
	if (mutex->def) {
		slowpath = 0;
		self = (pthread_t)0x12141968;
	} else 
#endif /* DEBUG */
	{
		slowpath = 1;
		self = pthread_self();
	}

        if (!TRY_LOCK(mutex->lock)) {
            return (EBUSY);
        }

        if(slowpath && (mutex->owner == self) && (mutex->type == PTHREAD_MUTEX_RECURSIVE )) {
		if (mutex->lock_count >= USHRT_MAX) {
               		UNLOCK(mutex->lock);
			return(EAGAIN);
		}
               mutex->lock_count++;
               UNLOCK(mutex->lock);
               return(ESUCCESS);
        } 
        
        if (mutex->waiters ||
		((mutex->owner != (pthread_t)NULL) && (mutex->cond_lock == 0)))
        {
                UNLOCK(mutex->lock);
                return (EBUSY);
        } else {
#if defined(DEBUG)
                _pthread_mutex_add(mutex, self);
#endif
                mutex->owner = (pthread_t)self;
		if (mutex->cond_lock) {
                    PTHREAD_MACH_CALL(semaphore_wait(mutex->sem), kern_res);
                    mutex->cond_lock = 0;
                    restore_sem_to_pool(mutex->sem);
                    mutex->sem = MACH_PORT_NULL;
		}
                if (slowpath && (mutex->type == PTHREAD_MUTEX_RECURSIVE))
                    mutex->lock_count = 1;
                UNLOCK(mutex->lock);
                return (ESUCCESS);
        }
}

/*
 * Unlock a mutex.
 * TODO: Priority inheritance stuff
 */
int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
        kern_return_t kern_res;
        int waiters;
        pthread_t self;
	int slowpath;
        
        if (mutex->sig == _PTHREAD_MUTEX_SIG_init)
        {
                int res;
                if (res = pthread_mutex_init(mutex, NULL))
                        return (res);
        }
        if (mutex->sig != _PTHREAD_MUTEX_SIG)
                return (EINVAL);        /* Not a mutex variable */

#if !defined(DEBUG)
	if (mutex->def) {
		slowpath = 0;
		self = (pthread_t)0x12141968;
	} else 
#endif /* DEBUG */
	{
		slowpath = 1;
		self = pthread_self();
	}

        LOCK(mutex->lock);

#if defined(DEBUG)
	if (mutex->owner != self)
#else
        if (slowpath 
		&& ((mutex->type == PTHREAD_MUTEX_ERRORCHECK ) 
             		|| (mutex->type == PTHREAD_MUTEX_RECURSIVE )) 
             	&& (mutex->owner != self)) 
#endif /* DEBUG */
	{
               UNLOCK(mutex->lock);
#if defined(DEBUG)
		abort();
#endif
               return(EPERM);
        }
        
        if (slowpath && (mutex->type == PTHREAD_MUTEX_RECURSIVE)  && --mutex->lock_count) {
                UNLOCK(mutex->lock);
		return (ESUCCESS);
            
        } else {
#if defined(DEBUG)
                _pthread_mutex_remove(mutex, mutex->owner);
#endif /* DEBUG */
                waiters = mutex->waiters;
                mutex->owner = (pthread_t)NULL;
		UNLOCK(mutex->lock);
                if (waiters)
                {
		    PTHREAD_MACH_CALL(semaphore_signal(mutex->sem), kern_res);
                }
                return (ESUCCESS);
        
        }
}

/*
 * Fetch the priority ceiling value from a mutex variable.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutex_getprioceiling(const pthread_mutex_t *mutex,
                             int *prioceiling)
{
        if (mutex->sig == _PTHREAD_MUTEX_SIG)
        {
                *prioceiling = mutex->prioceiling;
                return (ESUCCESS);
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}

/*
 * Set the priority ceiling for a mutex.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutex_setprioceiling(pthread_mutex_t *mutex,
                             int prioceiling,
                             int *old_prioceiling)
{
        if (mutex->sig == _PTHREAD_MUTEX_SIG)
        {
                if ((prioceiling >= -999) ||
                    (prioceiling <= 999))
                {
                        *old_prioceiling = mutex->prioceiling;
                        mutex->prioceiling = prioceiling;
                        return (ESUCCESS);
                } else
                {
                        return (EINVAL); /* Invalid parameter */
                }
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}

/*
 * Destroy a mutex attribute structure.
 */
int
pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
        attr->sig = _PTHREAD_NO_SIG;  /* Uninitialized */
        return (ESUCCESS);
}

/*
 * Get the priority ceiling value from a mutex attribute structure.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *attr,
                                 int *prioceiling)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                *prioceiling = attr->prioceiling;
                return (ESUCCESS);
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}

/*
 * Get the mutex 'protocol' value from a mutex attribute structure.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr,
                              int *protocol)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                *protocol = attr->protocol;
                return (ESUCCESS);
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}
/*
 * Get the mutex 'type' value from a mutex attribute structure.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutexattr_gettype(const pthread_mutexattr_t *attr,
                              int *type)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                *type = attr->type;
                return (ESUCCESS);
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}

/*
 *
 */
int
pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                *pshared = (int)PTHREAD_PROCESS_PRIVATE;
                return (ESUCCESS);
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}

/*
 * Initialize a mutex attribute structure to system defaults.
 */
int
pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
        attr->prioceiling = _PTHREAD_DEFAULT_PRIOCEILING;
        attr->protocol = _PTHREAD_DEFAULT_PROTOCOL;
        attr->type = PTHREAD_MUTEX_DEFAULT;
        attr->sig = _PTHREAD_MUTEX_ATTR_SIG;
        return (ESUCCESS);
}

/*
 * Set the priority ceiling value in a mutex attribute structure.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr,
                                 int prioceiling)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                if ((prioceiling >= -999) ||
                    (prioceiling <= 999))
                {
                        attr->prioceiling = prioceiling;
                        return (ESUCCESS);
                } else
                {
                        return (EINVAL); /* Invalid parameter */
                }
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}

/*
 * Set the mutex 'protocol' value in a mutex attribute structure.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr,
                              int protocol)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                if ((protocol == PTHREAD_PRIO_NONE) ||
                    (protocol == PTHREAD_PRIO_INHERIT) ||
                    (protocol == PTHREAD_PRIO_PROTECT))
                {
                        attr->protocol = protocol;
                        return (ESUCCESS);
                } else
                {
                        return (EINVAL); /* Invalid parameter */
                }
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}
/*
 * Set the mutex 'type' value in a mutex attribute structure.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutexattr_settype(pthread_mutexattr_t *attr,
                              int type)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                if ((type == PTHREAD_MUTEX_NORMAL) ||
                    (type == PTHREAD_MUTEX_ERRORCHECK) ||
                    (type == PTHREAD_MUTEX_RECURSIVE) ||
                    (type == PTHREAD_MUTEX_DEFAULT))
                {
                        attr->type = type;
                        return (ESUCCESS);
                } else
                {
                        return (EINVAL); /* Invalid parameter */
                }
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}

/*
 *
 */
int
pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                if (pshared == PTHREAD_PROCESS_PRIVATE)
                {
                        /* attr->pshared = protocol; */
                        return (ESUCCESS);
                } else
                {
                        return (EINVAL); /* Invalid parameter */
                }
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}

int mutex_try_lock(int *x) {
        return _spin_lock_try((pthread_lock_t *)x);
}

void mutex_wait_lock(int *x) {
        for (;;) {
                if( _spin_lock_try((pthread_lock_t *)x)) {
                        return;
                }
                swtch_pri(0);
        }
}

void 
cthread_yield(void) 
{
        sched_yield();
}

void 
pthread_yield_np (void) 
{
        sched_yield();
}

