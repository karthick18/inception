/*   INCEPTION: My tribute to Christopher Nolan
;    The code below tries to explain and follow the sequence of events that took place in Inception
;    as seen from the eyes of the programmer. It performs the inception through a clever trick
;    using x86 code morphing technique in inception.h. This is done so as to make it appear that the thought
;    about copying the inception string was done by Fischer himself even though he expected to return to 
;    a different state in real life.
;
;    Copyright (C) 2010-2011 A.R.Karthick 
;    <a.r.karthick@gmail.com, a_r_karthic@users.sourceforge.net>
;
;    This program is free software; you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation; either version 2 of the License, or
;    (at your option) any later version.
;
;    This program is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License
;    along with this program; if not, write to the Free Software
;    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
;
;
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <sched.h>
#include <sys/mman.h>
#include <time.h>

#ifdef __linux__
#include <syscall.h>
#define GET_TID  syscall(SYS_gettid)
#else
#define GET_TID  ({0;})
#endif

#include "list.h"
#include "inception_arch.h"

#define DREAM_INCEPTION_TARGET 0x1
#define DREAM_INCEPTION_PERFORMER 0x2
#define DREAM_WORLD_ARCHITECT 0x4
#define DREAM_ORGANIZER 0x8
#define DREAM_SHAPES_FAKER 0x10
#define DREAM_SEDATIVE_CREATOR 0x20
#define DREAM_OVERLOOKER 0x40
#define DREAM_ROLE_MASK 0x7f
#define DREAMERS (0x7)

/*
 * fprintf output buffer is thread-safe anyway. So don't care much
 */
#define output(fmt, arg...) do { fprintf(stdout, fmt, ##arg);} while(0)

#define DREAM_LEVELS (0x3 + 1 ) /* + 1 as an illustrative considering the 4th is really a limbo from 3rd */

struct dreamer_request
{
#define DREAMER_HIJACKED 0x1
#define DREAMER_DEFENSE_PROJECTIONS (0x2)
#define DREAMER_FREE_FALL (0x4)
#define DREAMER_FAKE_SHAPE (0x8)
#define DREAMER_SHOT (0x10)
#define DREAMER_KILLED (0x20)
#define DREAMER_NEXT_LEVEL (0x40)
#define DREAMER_IN_LIMBO (0x80)
#define DREAMER_IN_MY_DREAM (0x100) /*shared dream*/
#define DREAMER_KICK_BACK (0x200) 
#define DREAMER_FIGHT (0x400)
#define DREAMER_SELF (0x800)
#define DREAMER_FALL (0x1000)
#define DREAMER_SYNCHRONIZE_KICK (0x2000)
#define DREAMER_RECOVER (0x4000)

    struct dreamer_attr *dattr; /*dreamer attribute*/
    int cmd; /* request cmd */
    void *arg; /* request cmd arg*/
    struct list list; /* list head marker*/
};

struct dreamer_attr
{
    const char *name;
    int role;
    int shared_state; /* shared request command state*/
    int level; /*dreamer level*/
    struct list_head request_queue; /* per dreamer request queue*/
    struct list list; /* list head marker*/
    pthread_mutex_t mutex;
    pthread_cond_t *cond[DREAM_LEVELS]; /* per dreamer wake up levels */
};

static pthread_mutex_t inception_reality_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t inception_reality_wakeup_for_all = PTHREAD_COND_INITIALIZER;
static struct list_head dreamer_queue[DREAM_LEVELS];
static pthread_mutex_t dreamer_mutex[DREAM_LEVELS];
static pthread_mutex_t limbo_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t limbo_cond = PTHREAD_COND_INITIALIZER;

#define _INCEPTION_C_
#include "inception.h"

static char *fischers_mind_state;
static struct dreamer_attr *fischer_level1;
static pid_t fischer_level1_taskid; /*fischers level1 taskid*/
static int dream_delay_map[DREAM_LEVELS] = { 1, 2, 4, 8};
static int dreamers_in_reality;

static void fischer_dream_level1(void) __attribute__((unused));
/*
 * Queue the command to the dreamers request queue
 */
static void __dream_enqueue_cmd(struct dreamer_attr *dattr, int cmd, void *arg, int level, int locked)
{
    struct dreamer_request *req = calloc(1, sizeof(*req));
    assert(req != NULL);
    assert(level > 0 && level <= DREAM_LEVELS);
    req->dattr = dattr;
    req->cmd = cmd;
    req->arg = arg;
    if(!locked)
        pthread_mutex_lock(&dattr->mutex);
    list_add_tail(&req->list, &dattr->request_queue); 
    pthread_cond_signal(dattr->cond[level-1]);
    if(!locked)
        pthread_mutex_unlock(&dattr->mutex);
}

static __inline__ void dream_enqueue_cmd(struct dreamer_attr *dattr, int cmd, void *arg, int level)
{
    return __dream_enqueue_cmd(dattr, cmd, arg, level, 0);
}

static __inline__ void dream_enqueue_cmd_safe(struct dreamer_attr *dattr, int cmd, 
                                              void *arg, int level, pthread_mutex_t *dreamer_lock)
{
    /*
     * Drop the current level dreamer lock before reacquiring.
     */
    pthread_mutex_unlock(dreamer_lock);
    __dream_enqueue_cmd(dattr, cmd, arg, level, 0);
    pthread_mutex_lock(dreamer_lock);
}

static __inline__ void dream_enqueue_cmd_locked(struct dreamer_attr *dattr, int cmd, void *arg, int level)
{
    return __dream_enqueue_cmd(dattr, cmd, arg, level, 1);
}

/*
 * Clone the request command to all the dreamers: Called with lock held.
 */
static void dream_clone_cmd(struct list_head *dreamer_queue, int cmd, void *arg, struct dreamer_attr *dattr, int level)
{
    register struct list *iter;
    for(iter = dreamer_queue->head; iter; iter = iter->next)
    {
        struct dreamer_attr *dreamer = LIST_ENTRY(iter, struct dreamer_attr, list);
        if(dattr && dreamer == dattr)
            continue; /*skip cloning it on this dreamer*/
        dream_enqueue_cmd(dreamer, cmd, arg, level);
    }
}

static struct dreamer_request *dream_dequeue_cmd_locked(struct dreamer_attr *dattr)
{
    struct dreamer_request *req = NULL;
    struct list *head = NULL;
    if(!dattr->request_queue.nodes) 
        return NULL;
    head = dattr->request_queue.head;
    assert(head != NULL);
    req = LIST_ENTRY(head, struct dreamer_request, list);
    list_del(head, &dattr->request_queue);
    return req;
}

static __inline__ struct dreamer_request *dream_dequeue_cmd(struct dreamer_attr *dattr)
{
    struct dreamer_request *req;
    pthread_mutex_lock(&dattr->mutex);
    req = dream_dequeue_cmd_locked(dattr);
    pthread_mutex_unlock(&dattr->mutex);
    return req;
}

static struct dreamer_attr *dreamer_find(struct list_head *dreamer_queue, const char *name, int role)
{
    register struct list *iter;
    for(iter = dreamer_queue->head; iter ; iter = iter->next)
    {
        struct dreamer_attr *dattr = LIST_ENTRY(iter, struct dreamer_attr, list);
        if(role && (dattr->role & role))
            return dattr;
        if(name &&
           !strcasecmp(dattr->name, name))
            return dattr;
    }
    return NULL;
}

static struct dreamer_attr *dreamer_find_sync_locked(struct dreamer_attr *dreamer, int level, const char *name, int role)
{
    struct dreamer_attr *dattr = NULL;
    if(!level) return NULL;
    rescan:
    dattr = dreamer_find(&dreamer_queue[level-1], name, role);
    if(!dattr)
    {
        static int c;
        pthread_mutex_unlock(&dreamer_mutex[level-1]);
        if(++c >= 10)
        {
            output("[%s] waiting for [%s] to join at level [%d]\n", dreamer->name,
                   name ?:"Unknown", level);
        }
        usleep(100000);
        pthread_mutex_lock(&dreamer_mutex[level-1]);
        goto rescan;
    }
    return dattr;
}

static struct dreamer_attr *dreamer_find_sync(struct dreamer_attr *dreamer, int level, const char *name, int role)
{
    struct dreamer_attr *dattr;
    if(!level) return NULL;
    pthread_mutex_lock(&dreamer_mutex[level-1]);
    dattr = dreamer_find_sync_locked(dreamer, level, name, role);
    pthread_mutex_unlock(&dreamer_mutex[level-1]);
    return dattr;
}

/*
 * In a dream, you run 12 times slower : 5 mins of realtime = 60 mins
 * Fake the slowness by reduction in threads priority or the 
 * scheduling priority of the dreamer
 */

static void set_thread_priority(struct dreamer_attr *dattr, int level)
{
    struct sched_param dream_param = {0};
    int policy = 0;

    assert(pthread_getschedparam(pthread_self(), &policy, &dream_param) == 0);
    if(dream_param.sched_priority > 12)
        dream_param.sched_priority -= 12;

    output("Dreamer [%s], level [%d], priority [%d], policy [%s]\n",
           dattr->name, level, dream_param.sched_priority, 
           policy == SCHED_FIFO ? "FIFO" : 
           (policy == SCHED_RR ? "RR" : "OTHER"));

    assert(pthread_setschedparam(pthread_self(), policy, &dream_param) == 0);
    
}

/*
 * Wake up an individual dreamer in one level and let him propagate the kick down to levels below.
 */
static void wake_up_dreamer(struct dreamer_attr *dattr, int level)
{
    register struct list *iter;
    if(!level || (dattr->shared_state & DREAMER_IN_LIMBO)) return;
    pthread_mutex_lock(&dreamer_mutex[level-1]);
    for(iter = dreamer_queue[level-1].head; iter; iter = iter->next)
    {
        struct dreamer_attr *dreamer = LIST_ENTRY(iter, struct dreamer_attr, list);
        if( !(dreamer->role ^ dattr->role) )
        {
            dream_enqueue_cmd(dreamer, DREAMER_KICK_BACK, NULL, dreamer->level);
            break;
        }
    }
    pthread_mutex_unlock(&dreamer_mutex[level-1]);
}

/*
 * Level 0 is wake up dreamers on all levels.
 * Skip guys in a limbo from that level to all the way down.
 */
static void wake_up_dreamers(int level)
{
    int start = DREAM_LEVELS-1,end = 0;
    register int i;
    if(level > 0)
    {
        start = level - 1;
        end = level - 1;
    }
    for(i = start; i >= end; --i)
    {
        struct list *iter;
        pthread_mutex_lock(&dreamer_mutex[i]);
        for(iter = dreamer_queue[i].head; iter; iter = iter->next)
        {
            struct dreamer_attr *dattr = LIST_ENTRY(iter, struct dreamer_attr, list);
            if( (dattr->shared_state & DREAMER_IN_LIMBO) )
                continue;
            dream_enqueue_cmd(dattr, DREAMER_KICK_BACK, NULL, dattr->level);
        }
        pthread_mutex_unlock(&dreamer_mutex[i]);
    }
}

/*
 * Update states on all the levels and down.
 */
static void set_state(struct dreamer_attr *dattr, int state)
{
    register int i;
    for(i = DREAM_LEVELS - 1; i >= 0; --i)
    {
        register struct list *iter;
        pthread_mutex_lock(&dreamer_mutex[i]);
        for(iter = dreamer_queue[i].head; iter; iter = iter->next)
        {
            struct dreamer_attr *dreamer = LIST_ENTRY(iter, struct dreamer_attr, list);
            if(!(dreamer->role ^ dattr->role))
            {
                dreamer->shared_state |= state;
                break;
            }
        }
        pthread_mutex_unlock(&dreamer_mutex[i]);
    }
}

/*
 * Set the limbo state on all levels of this dreamer so he cannot get a kick back
 * Called with the lock on that level.
 */
static void set_limbo_state(struct dreamer_attr *dattr)
{
    set_state(dattr, DREAMER_IN_LIMBO);
}

/*
 * Called with the dreamer attr mutex held for the appropriate level.
 */
static void wait_for_kick(struct dreamer_attr *dattr)
{
    struct dreamer_request *req = NULL;
    for(;;)
    {
        struct timespec ts = {0};
        while ( (req = dream_dequeue_cmd_locked(dattr)) )
        {
            if(req->cmd == DREAMER_KICK_BACK)
            {
                free(req);
                if(dattr->level > 1)
                {
                    output("[%s] got Kick at level [%d]. Exiting back to level [%d]\n",
                           dattr->name, dattr->level, dattr->level - 1);
                }
                else
                {
                    output("[%s] got Kick at level [%d]. Exiting back to reality\n",
                           dattr->name, dattr->level);
                }
                goto out;
            }
            free(req);
        }
        arch_gettime(dream_delay_map[dattr->level-1], &ts);
        pthread_cond_timedwait(dattr->cond[dattr->level-1], &dattr->mutex, &ts);
    }
    out:
    return;
}

static struct dreamer_attr *dream_attr_clone(int level, struct dreamer_attr *dattr)
{
    struct dreamer_attr *dattr_clone = calloc(1, sizeof(*dattr_clone));
    assert(dattr_clone != NULL);
    memcpy(dattr_clone, dattr, sizeof(*dattr_clone));
    dattr_clone->level = level;
    memset(&dattr_clone->mutex, 0, sizeof(dattr_clone->mutex));
    assert(pthread_mutex_init(&dattr_clone->mutex, NULL) == 0);
    list_init(&dattr_clone->request_queue);
    return dattr_clone;
}

static __inline__ void dream_level_create(int level, void * (*dream_function) (void *), struct dreamer_attr *dattr)
{
    pthread_attr_t attr;
    pthread_t dream;
    struct dreamer_attr *dattr_clone = dream_attr_clone(level, dattr);
    assert(dattr_clone != NULL);
    assert(pthread_attr_init(&attr)==0);
    assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0);
    assert(pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED)==0);
    assert(pthread_create(&dream, &attr, dream_function, dattr_clone) == 0);
}

/*
 * Now this is the state where Cobb. meets Saito.
 * The beauty of the Films ending is: Did Cobb take a kick back to reality on seeing Saito remind him
 * that he has been in a limbo with his wife and has to come back to become Young.
 * And does Saito take the kick back when Cobb. pulls the trigger on him implicitly in limbo to give him the kick back.
 * Either way based on whether Cobb. got the kick back from limbo or not, the end is a reality or limbo.
 * Thats the ingenuity of Inception. So I think its better if we don't mess this up for ourselves and
 * just sleep here till infinity!
 */

static void infinite_subconsciousness(struct dreamer_attr *dattr)
{
    struct timespec ts = {0};
    static int dreamers;

    pthread_mutex_lock(&limbo_mutex);
    ++dreamers;
    while(dreamers != 2) 
    {
        arch_gettime(2, &ts);
        pthread_cond_timedwait(&limbo_cond, &limbo_mutex, &ts);
    }
    /*
     * Wait for the signal from Fischer
     */
    while(!dreamers_in_reality)
    {
        arch_gettime(1, &ts);
        pthread_cond_timedwait(&limbo_cond, &limbo_mutex, &ts);
    }

    if((dattr->role & DREAM_INCEPTION_PERFORMER))
    {
        output("\n\nCobbs' search for Saito ends in Limbo. Now either both take the kick back to reality "
               "or the ending is still a state of limbo from Cobbs' perspective, which is what Nolan wants us to think\n");
        output("This is in spite of witnessing his children turn towards him for the first time which we're never shown in his projections.\n");
        output("So, let me end the limbo state abruptly like the Movie with the totem spinning and leave it to the reviewers to decide the infinite sleep:-)\n\n");
    }
    pthread_cond_signal(&limbo_cond);
    pthread_mutex_unlock(&limbo_mutex);
    sleep(1<<31);
}

/*
 * Limbo is a state of infinite subconciousness
 */
static void enter_limbo(struct dreamer_attr *dattr)
{
    struct dreamer_attr *clone = NULL;
    struct dreamer_request *req = NULL;

    pthread_mutex_lock(&dattr->mutex);
    dattr->shared_state |= DREAMER_IN_LIMBO;
    clone = dream_attr_clone(dattr->level+1, dattr);
    pthread_mutex_unlock(&dattr->mutex);

    assert(clone != NULL);
    pthread_mutex_lock(&dreamer_mutex[3]);
    list_add_tail(&clone->list, &dreamer_queue[3]);
    while( (dreamer_queue[3].nodes + 3) != DREAMERS)
    {
        pthread_mutex_unlock(&dreamer_mutex[3]);
        usleep(100000);
        pthread_mutex_lock(&dreamer_mutex[3]);
    }
    pthread_mutex_unlock(&dreamer_mutex[3]);

    switch( (clone->role & DREAM_ROLE_MASK) )
    {
    case DREAM_INCEPTION_PERFORMER: /* Cobb */
        {
            struct timespec ts;
            struct dreamer_attr *ariadne = NULL;
            int inception_done = 0;
            int search_for_saito = 0;
            ariadne = dreamer_find(&dreamer_queue[3], "ariadne", DREAM_WORLD_ARCHITECT);
            /*
             * Self enqueue Mal and her thoughts into the dream
             */
            dream_enqueue_cmd(clone, DREAMER_IN_MY_DREAM, (void*)"Cobb takes Elevator to meet Mal", clone->level);
            dream_enqueue_cmd(clone, DREAMER_IN_MY_DREAM, 
                              (void*)"[Cobb] tells [Mal] about his inception on her to think that the WORLD is unreal", 
                              clone->level);
            dream_enqueue_cmd(clone, DREAMER_IN_MY_DREAM,
                              (void*)"[Mal] wants [Cobb] to go back with him into the world they built in their dreams",
                              clone->level);
            pthread_mutex_lock(&clone->mutex);
            for(;;)
            {
                while( (req = dream_dequeue_cmd_locked(clone)) )
                {
                    /*
                     * Meets Mal
                     */
                    if(req->cmd == DREAMER_IN_MY_DREAM)
                    {
                        output("%s in level [%d] while in limbo\n", (const char*)req->arg, clone->level);
                    }
                    /*
                     * Mal killed
                     */
                    else if(req->cmd == DREAMER_KILLED)
                    {
                        output("[%s] finds %s in level [%d] while in limbo\n", 
                               clone->name, (const char*)req->arg, clone->level);
                    }
                    /*
                     * Recover to search for Saito. Here mark the inception
                     */
                    else if(req->cmd == DREAMER_RECOVER)
                    {
                        struct dreamer_attr *source = (struct dreamer_attr*)req->arg;
                        /*
                         * If the recovery trigger is from ariadne
                         */
                        if( (source->role & DREAM_WORLD_ARCHITECT) )
                        {
                            if(!inception_done)
                            {
                                search_for_saito = 1;
                            }
                            else
                            {
                                pthread_mutex_unlock(&clone->mutex);
                                search_saito:
                                output("[%s] enters limbo to search for Saito in limbo at level [%d]\n",
                                       clone->name, clone->level);
                                set_limbo_state(clone);
                                usleep(10000);
                                infinite_subconsciousness(clone);
                                pthread_mutex_lock(&clone->mutex);
                                output("[%s] returned after searching for Saito in limbo at level [%d]\n",
                                       clone->name, clone->level);
                                assert(0); /* should not return back here*/
                            }
                        }
                        /*
                         * Indicator from Fischer for the final shot.
                         */
                        else if( (source->role & DREAM_INCEPTION_TARGET) )
                        {
                            pthread_mutex_unlock(&clone->mutex);
                            inception_done = 1;
                            memcpy(fischers_mind_state, inception_thoughts, sizeof(inception_thoughts));
                            /*
                             * Send recovery indicator to Ariadne
                             */
                            dream_enqueue_cmd(ariadne, DREAMER_RECOVER, clone, ariadne->level);
                            if(search_for_saito)
                            {
                                goto search_saito;
                            }
                            pthread_mutex_lock(&clone->mutex);
                        }
                    }
                }
                arch_gettime(dream_delay_map[clone->level-1], &ts);
                pthread_cond_timedwait(clone->cond[3], &clone->mutex, &ts);
            }
        }
        break;

    case DREAM_WORLD_ARCHITECT: /* Ariadne */
        {
            struct dreamer_attr *cobb = NULL;
            struct dreamer_attr *fischer = NULL;
            struct timespec ts = {0};
            cobb = dreamer_find(&dreamer_queue[3], "cobb", DREAM_INCEPTION_PERFORMER);
            fischer = dreamer_find(&dreamer_queue[3], "fischer", DREAM_INCEPTION_TARGET);
            /*  
             * Self enqueue to follow Cobb. in the Elevator to his wife.
             */
            dream_enqueue_cmd(clone, DREAMER_IN_MY_DREAM, cobb, clone->level);
            pthread_mutex_lock(&clone->mutex);
            for(;;)
            {
                while ( (req = dream_dequeue_cmd_locked(clone) ) )
                {
                    if(req->cmd == DREAMER_IN_MY_DREAM)
                    {
                        output("[%s] follows [%s] in Elevator to level [%d] in Limbo to meet his wife\n",
                               clone->name, cobb->name, clone->level);
                        pthread_mutex_unlock(&clone->mutex);
                        /*
                         * Take a breather while Cobb. interacts with his and tells her about his inception.
                         */
                        sleep(dream_delay_map[clone->level-1]);
                        dream_enqueue_cmd(cobb, DREAMER_KILLED, (void*)"[Mal] killed", cobb->level);
                        /*
                         * Quick breather
                         */
                        usleep(10000);
                        /*
                         * Now send Fischer a kick back from limbo down to reconcile.
                         */
                        dream_enqueue_cmd(fischer, DREAMER_KICK_BACK, clone, fischer->level);
                        /*
                         * Now tell Cobb. to recover and go and search Saito as he is the only one who can
                         * search her in limbo.
                         */
                        output("[%s] tells [%s] to search for Saito in limbo at level [%d]\n",
                               clone->name, cobb->name, clone->level);
                        dream_enqueue_cmd(cobb, DREAMER_RECOVER, clone, cobb->level);
                        pthread_mutex_lock(&clone->mutex);
                    }
                    else if(req->cmd == DREAMER_RECOVER)
                    {
                        /*
                         * Indication for us to take the kick back.
                         */
                        dream_enqueue_cmd_safe(clone, DREAMER_KICK_BACK, clone, clone->level, &clone->mutex);
                    }
                    else if(req->cmd == DREAMER_KICK_BACK)
                    {
                        struct dreamer_attr *self = NULL; /*ourself in lower level*/
                        /*
                         * Return back
                         */
                        dattr->shared_state &= ~DREAMER_IN_LIMBO;
                        clone->shared_state &= ~DREAMER_IN_LIMBO;
                        pthread_mutex_unlock(&clone->mutex);
                        free(req);
                        usleep(10000);
                        self = dreamer_find_sync(clone, clone->level-1, "ariadne", DREAM_WORLD_ARCHITECT);
                        dream_enqueue_cmd(self, DREAMER_KICK_BACK, clone, self->level);
                        output("[%s] taking the kick back from limbo at level [%d] to level [%d]\n",
                               clone->name, clone->level, clone->level-1);
                        goto out;
                    }
                    free(req);
                }
                arch_gettime(dream_delay_map[clone->level-1], &ts);
                pthread_cond_timedwait(clone->cond[3], &clone->mutex, &ts);
            }
        }
        break;

    case DREAM_OVERLOOKER: /* Saito */
        {
            set_limbo_state(clone);
            usleep(1000);
            infinite_subconsciousness(clone);
        }
        break;

    case DREAM_INCEPTION_TARGET: /*Fischer*/
        {
            struct dreamer_attr *self = NULL;
            struct timespec ts = {0};
            /*
             * Find ourselves in the lower level to take the kick back.
             */
            self = dreamer_find_sync(clone, clone->level-1, "fischer", DREAM_INCEPTION_TARGET);
            pthread_mutex_lock(&clone->mutex);
            for(;;)
            {
                while ( (req = dream_dequeue_cmd_locked(clone)) )
                {
                    if(req->cmd == DREAMER_KICK_BACK)
                    {
                        dattr->shared_state &= ~DREAMER_IN_LIMBO;
                        clone->shared_state &= ~DREAMER_IN_LIMBO;
                        pthread_mutex_unlock(&clone->mutex);
                        free(req);
                        dream_enqueue_cmd(self, DREAMER_KICK_BACK, clone, self->level);
                        output("[%s] kicking off from limbo at [%d] to level [%d]\n",
                               clone->name, clone->level, clone->level-1);
                        goto out;
                    }
                    free(req);
                }
                arch_gettime(dream_delay_map[clone->level-1], &ts);
                pthread_cond_timedwait(clone->cond[3], &clone->mutex, &ts);
            }
        }
        break;
    }

    out:
    return ; 
}


/*
 * The last level of the dream beyond which we enter limbo.
 */
static void *dream_level_3(void *arg)
{
    struct dreamer_attr *dattr = arg;
    struct dreamer_request *req = NULL;
    assert(dattr->level == 3);
    set_thread_priority(dattr, 3);
    pthread_mutex_lock(&dreamer_mutex[2]);
    list_add_tail(&dattr->list, &dreamer_queue[2]);
    while( (dreamer_queue[2].nodes + 2 != DREAMERS ) )
    {
        pthread_mutex_unlock(&dreamer_mutex[2]);
        usleep(10000);
        pthread_mutex_lock(&dreamer_mutex[2]);
    }
    pthread_mutex_unlock(&dreamer_mutex[2]);
    /*
     * All have joined in level 3
     */
    switch( (dattr->role & DREAM_ROLE_MASK) )
    {
    case DREAM_INCEPTION_PERFORMER: /* Cobb */
        {
            struct dreamer_attr *fischer = NULL;
            struct dreamer_attr *ariadne = NULL;
            struct dreamer_attr *eames = NULL;
            struct timespec ts = {0};
            /*
             * Self enqueue
             */
            fischer = dreamer_find(&dreamer_queue[2], "fischer", DREAM_INCEPTION_TARGET);
            ariadne = dreamer_find(&dreamer_queue[2], "ariadne", DREAM_WORLD_ARCHITECT);
            eames = dreamer_find(&dreamer_queue[2], "eames", DREAM_SHAPES_FAKER);
            dream_enqueue_cmd(dattr, DREAMER_FIGHT, (void*)"Fischer", dattr->level);
            dream_enqueue_cmd(dattr, DREAMER_IN_MY_DREAM, (void*)"Mal", dattr->level);
            pthread_mutex_lock(&dattr->mutex);
            for(;;)
            {
                while ( (req = dream_dequeue_cmd_locked(dattr)))
                {
                    if(req->cmd == DREAMER_FIGHT)
                    {
                        output("[%s] fights [%s] defense projections in level [%d]\n",
                               dattr->name, (char*)req->arg, dattr->level);
                    }
                    else if(req->cmd == DREAMER_IN_MY_DREAM)
                    {
                        pthread_mutex_unlock(&dattr->mutex);
                        output("[%s] sees his wife [%s] in his dream. [%s] shoots Fischer\n",
                               dattr->name, (char*)req->arg, (char*)req->arg);
                        dream_enqueue_cmd(fischer, DREAMER_SHOT, (void*)"Mal", fischer->level);
                        /*
                         * Let Eames know so he could start recovery on Fischer
                         */
                        dream_enqueue_cmd(eames, DREAMER_SHOT, fischer, dattr->level);
                        /*
                         * Hint to Ariadne about Fischers death from Mal's hands
                         * Take the dreamer mutex for a synchronized reply wait.
                         */
                        arch_gettime(dream_delay_map[dattr->level-1], &ts);
                        pthread_mutex_lock(&dreamer_mutex[dattr->level-1]);
                        dream_enqueue_cmd(ariadne, DREAMER_SHOT, (void*)"Fischer shot by Mal", dattr->level);
                        pthread_cond_timedwait(dattr->cond[dattr->level-1], 
                                               &dreamer_mutex[dattr->level-1], &ts);
                        pthread_mutex_unlock(&dreamer_mutex[dattr->level-1]);
                        pthread_mutex_lock(&dattr->mutex);
                    }
                    else if(req->cmd == DREAMER_NEXT_LEVEL)
                    {
                        pthread_mutex_unlock(&dattr->mutex);
                        output("[%s] follows [%s] and enters limbo with his Wifes projections in level [%d]\n",
                               dattr->name, ((struct dreamer_attr*)req->arg)->name, dattr->level);
                        enter_limbo(dattr);
                        pthread_mutex_lock(&dattr->mutex);
                        /*
                         * should not be reached
                         */
                        output("[%s] returned from limbo. Exiting out at level [%d]\n", dattr->name, 
                               dattr->level);
                        exit(0);
                    }
                    free(req);
                }
                pthread_mutex_unlock(&dattr->mutex);
                sleep(2);
                pthread_mutex_lock(&dattr->mutex);
            }
        }
        break;

    case DREAM_WORLD_ARCHITECT: /* Ariadne*/
        {
            /*
             * Wait for Cobbs command to enter his dream in limbo with him.
             */
            struct timespec ts = {0};
            struct dreamer_attr *cobb = NULL;
            int ret_from_limbo = 0;
            cobb = dreamer_find(&dreamer_queue[2], "cobb", DREAM_INCEPTION_PERFORMER);
            pthread_mutex_lock(&dattr->mutex);
            for(;;)
            {
                while ( (req = dream_dequeue_cmd_locked(dattr)) )
                {
                    if(req->cmd == DREAMER_SHOT)
                    {
                        pthread_mutex_unlock(&dattr->mutex);
                        output("[%s] sees %s in level [%d]\n", dattr->name, 
                               (char*)req->arg, dattr->level);
                        output("[%s] tells [%s] to follow Fischer to level [%d] in Mal's world in limbo\n",
                               dattr->name, cobb->name, dattr->level+1);
                        pthread_mutex_lock(&dreamer_mutex[dattr->level-1]);
                        dream_enqueue_cmd(cobb, DREAMER_NEXT_LEVEL, dattr, cobb->level);
                        pthread_mutex_unlock(&dreamer_mutex[dattr->level-1]);
                        output("[%s] enters Limbo at level [%d]\n",
                               dattr->name, dattr->level+1);
                        enter_limbo(dattr);
                        pthread_mutex_lock(&dattr->mutex);
                    }
                    else if(req->cmd == DREAMER_KICK_BACK)
                    {
                        if(!ret_from_limbo)
                        {
                            struct dreamer_attr *yusuf = NULL;
                            ret_from_limbo = 1;
                            output("[%s] returned from Fischers limbo to level [%d] to take the synchronized kick\n", 
                                   dattr->name, dattr->level);
                            pthread_mutex_unlock(&dattr->mutex);
                            yusuf = dreamer_find_sync(dattr, 1, "yusuf", DREAM_SEDATIVE_CREATOR);
                            dream_enqueue_cmd(yusuf, DREAMER_SYNCHRONIZE_KICK, dattr, yusuf->level);
                            /*
                             * Take a breather while Yusuf does his work so we can rescan for a kick back
                             * Otherwise we miss and get it after our delayed sleep
                             */
                            usleep(10000);
                            pthread_mutex_lock(&dattr->mutex);
                        }
                        else
                        {
                            free(req);
                            output("[%s] got Kick back from level [%d]. Exiting back to level [%d]\n",
                                   dattr->name, dattr->level, dattr->level-1);
                            goto out_unlock;
                        }
                    }
                    free(req);
                }
                arch_gettime(dream_delay_map[dattr->level-1], &ts);
                pthread_cond_timedwait(dattr->cond[2], &dattr->mutex, &ts);
            }
        }
        break;

    case DREAM_SHAPES_FAKER: /* Eames*/
        {
            struct timespec ts = {0};
            struct dreamer_attr *fischer = NULL;
            struct dreamer_attr *saito = NULL;
            saito = dreamer_find(&dreamer_queue[2], "saito", DREAM_OVERLOOKER);
            /*
             * Self enqueue and he is the dreamer at this level
             */
            dream_enqueue_cmd(dattr, DREAMER_FIGHT, dattr, dattr->level);
            /*
             * Ask Saito to fight first before he is killed!
             */
            dream_enqueue_cmd(saito, DREAMER_FIGHT, dattr, dattr->level);
            pthread_mutex_lock(&dattr->mutex);
            for(;;)
            {
                while ( (req = dream_dequeue_cmd_locked(dattr)))
                {
                    if(req->cmd == DREAMER_FIGHT)
                    {
                        output("[%s] fights Fischers defense projections at level [%d]\n",
                               dattr->name, dattr->level);
                    }
                    else if(req->cmd == DREAMER_SHOT)
                    {
                        pthread_mutex_unlock(&dattr->mutex);
                        fischer = ( (struct dreamer_attr*)req->arg);
                        output("[%s] sees [%s] shot in level [%d]. Starts recovery\n",
                               dattr->name, fischer->name, dattr->level);
                        output("[%s] tells [%s] to keep fighting Fischers projections in level [%d]\n",
                               dattr->name, saito->name, dattr->level);
                        dream_enqueue_cmd(saito, DREAMER_FIGHT, dattr, saito->level);

                        dream_enqueue_cmd(dattr, DREAMER_RECOVER, fischer, dattr->level);
                        pthread_mutex_lock(&dattr->mutex);
                    }
                    else if(req->cmd == DREAMER_RECOVER)
                    {
                        output("[%s] doing recovery on [%s] who is shot at level [%d]\n",
                               dattr->name, ( (struct dreamer_attr*)req->arg)->name, dattr->level);
                        pthread_mutex_unlock(&dattr->mutex);
                        /*
                         * Dream about Saito getting killed ultimately as I am the dreamer in this level.
                         */
                        dream_enqueue_cmd(saito, DREAMER_KILLED, dattr, saito->level);
                        pthread_mutex_lock(&dattr->mutex);
                    }
                    else if(req->cmd == DREAMER_KICK_BACK)
                    {
                        struct dreamer_attr *src = (struct dreamer_attr*)req->arg;
                        if(src && (src->role & DREAM_INCEPTION_TARGET))
                        {
                            pthread_mutex_unlock(&dattr->mutex);
                            output("[%s] sees [%s] get a recovery kick at level [%d]. "
                                   "Starts faking Fischers Father's projections for the final Inception\n",
                                   dattr->name, src->name, src->level);
                            dream_enqueue_cmd(src, DREAMER_FAKE_SHAPE, "Maurice Fischer", src->level);
                            pthread_mutex_lock(&dattr->mutex);
                        }
                        else
                        {
                            free(req);
                            output("[%s] got Kick at level [%d]. Exiting back to level [%d]\n",
                                   dattr->name, dattr->level, dattr->level - 1);
                            goto out_unlock;
                        }
                    }
                    free(req);
                }
                if(fischer)
                {
                    /*
                     * Keep recovering fischer if he is shot in this level.
                     */
                    dream_enqueue_cmd_safe(dattr, DREAMER_RECOVER, fischer, dattr->level, &dattr->mutex);
                }
                arch_gettime(dream_delay_map[dattr->level-1], &ts);
                pthread_cond_timedwait(dattr->cond[2], &dattr->mutex, &ts);
            }
        }
        break;

    case DREAM_OVERLOOKER: /*Saito*/
        {
            struct timespec ts = {0};
            pthread_mutex_lock(&dattr->mutex);
            for(;;)
            {
                while( (req = dream_dequeue_cmd_locked(dattr)))
                {
                    if(req->cmd == DREAMER_FIGHT)
                    {
                        output("[%s] fights Fischers projections in level [%d]\n", 
                               dattr->name, dattr->level);
                    }
                    else if(req->cmd == DREAMER_KILLED) /* Killed. Enter limbo */
                    {
                        output("[%s] gets killed at level [%d]. Enters limbo\n", dattr->name, dattr->level);
                        pthread_mutex_unlock(&dattr->mutex);
                        /*
                         * Update killed status on all the levels. just for the sake of being
                         * consistent
                         */
                        set_state(dattr, DREAMER_KILLED);
                        enter_limbo(dattr);
                        pthread_mutex_lock(&dattr->mutex);
                        /*
                         * Unreached.
                         */
                        output("[%s] returned back from Limbo at level [%d]\n", dattr->name, dattr->level);
                        exit(0);
                    }
                    free(req);
                }
                arch_gettime(dream_delay_map[dattr->level-1], &ts);
                pthread_cond_timedwait(dattr->cond[2], &dattr->mutex, &ts);
            }
        }
        break;

    case DREAM_INCEPTION_TARGET: /* Fischer */
        {
            int reconciled = 0;
            struct timespec ts = {0};
            pthread_mutex_lock(&dattr->mutex);
            for(;;)
            {
                while ( (req = dream_dequeue_cmd_locked(dattr)))
                {
                    if(req->cmd == DREAMER_SHOT)
                    {
                        output("[%s] shot by [%s] in level [%d]\n", 
                               dattr->name, (char*)req->arg, dattr->level);
                        /*
                         * Freeze for sometime before joining Cobb and Ariadne in limbo.
                         */
                        pthread_mutex_unlock(&dattr->mutex);
                        usleep(100000);
                        enter_limbo(dattr);
                        pthread_mutex_lock(&dattr->mutex);
                    }
                    else if(req->cmd == DREAMER_KICK_BACK)
                    {
                        if(!reconciled)
                        {
                            struct dreamer_attr *eames = NULL;
                            pthread_mutex_unlock(&dattr->mutex);
                            output("[%s] got a kick back from Limbo at level [%d]\n", dattr->name, dattr->level);
                            eames = dreamer_find(&dreamer_queue[2], "eames", DREAM_SHAPES_FAKER);
                            dream_enqueue_cmd(eames, DREAMER_KICK_BACK, dattr, eames->level);
                            pthread_mutex_lock(&dattr->mutex);
                        }
                        else 
                        {
                            free(req);
                            output("[%s] got a kick at level [%d]. Falling back to level [%d]\n",
                                   dattr->name, dattr->level, dattr->level - 1);
                            goto out_unlock;
                        }
                     }
                    /*
                     * Fischer meeting with his dying father (reconciliation phase at the lowest level)
                     */
                    else if(req->cmd == DREAMER_FAKE_SHAPE)
                    {
                        struct dreamer_attr *cobb = NULL;
                        reconciled = 1;
                        pthread_mutex_unlock(&dattr->mutex);
                        usleep(10000); /*take a breather*/
                        output("[%s] going to meet his dying father [%s] after getting a kick back to level [%d]\n",
                               dattr->name, (const char*)req->arg, dattr->level);
                        /*
                         * Indicator to Cobb. for you know WHAT :-)
                         */
                        pthread_mutex_lock(&dreamer_mutex[dattr->level]);
                        cobb = dreamer_find_sync_locked(dattr, dattr->level+1, "cobb", DREAM_INCEPTION_PERFORMER);
                        dream_enqueue_cmd(cobb, DREAMER_RECOVER, dattr, cobb->level);
                        pthread_mutex_unlock(&dreamer_mutex[dattr->level]);
                        pthread_mutex_lock(&dattr->mutex);
                    }
                    free(req);
                }
                arch_gettime(dream_delay_map[dattr->level-1], &ts);
                pthread_cond_timedwait(dattr->cond[2], &dattr->mutex, &ts);
            }
        }
        break;
        
    default:
        break;
    }
    out_unlock:
    pthread_mutex_unlock(&dattr->mutex);
    wake_up_dreamer(dattr, 2);
    return NULL;
}

static void *dream_level_2(void *arg)
{
    struct dreamer_attr *dattr = arg;
    int dreamers = 0;
    assert(dattr->level == 2);
    set_thread_priority(dattr, 2);
    /*
     * take level 2 lock.
     */
    pthread_mutex_lock(&dreamer_mutex[1]);
    list_add_tail(&dattr->list, &dreamer_queue[1]);
    /*
     * Wait for the expected members to join at this level.
     */
    while( (dreamers = dreamer_queue[1].nodes)+1 != DREAMERS)
    {
        pthread_mutex_unlock(&dreamer_mutex[1]);
        usleep(10000);
        pthread_mutex_lock(&dreamer_mutex[1]);
    }

    pthread_mutex_unlock(&dreamer_mutex[1]);
    switch((dattr->role & DREAM_ROLE_MASK))
    {
    case DREAM_INCEPTION_PERFORMER: /* Cobb in level 2 */
        {
            /*
             * Wait for Ariadne and Fischer to join me. at this level after meeting with Arthur
             */
            struct dreamer_request *req;
            struct dreamer_attr *eames;
            struct timespec ts = {0};
            int wait_for_dreamers = DREAM_WORLD_ARCHITECT | DREAM_INCEPTION_TARGET;
            eames = dreamer_find(&dreamer_queue[1], "eames", DREAM_SHAPES_FAKER);
            pthread_mutex_lock(&dattr->mutex);
            while(wait_for_dreamers > 0)
            {
                while( (!(req = dream_dequeue_cmd_locked(dattr)) ) 
                       ||
                       req->cmd != DREAMER_IN_MY_DREAM 
                       ||
                       !req->arg
                       ||
                       !( ((struct dreamer_attr*)req->arg)->role & wait_for_dreamers)
                       )
                {
                    if(req) free(req);
                    arch_gettime(dream_delay_map[dattr->level-1], &ts);
                    pthread_cond_timedwait(dattr->cond[dattr->level-1], &dattr->mutex, &ts);
                }
                wait_for_dreamers &= ~((struct dreamer_attr*)req->arg)->role;
                output("[%s] taking [%s] to level 3\n", dattr->name, ((struct dreamer_attr*)req->arg)->name);
                dream_enqueue_cmd((struct dreamer_attr*)req->arg, DREAMER_NEXT_LEVEL, dattr, dattr->level);
                free(req);
            }
            /*
             * Ariadne + Fischer has joined. Go to level 3. myself. Take Eames into level 3
             */
            pthread_mutex_unlock(&dattr->mutex);
            dream_level_create(dattr->level + 1, dream_level_3, dattr);
            dream_enqueue_cmd(eames, DREAMER_NEXT_LEVEL, dattr, eames->level);
            pthread_mutex_lock(&dattr->mutex);
            /*
             * Just do nothing and wait for kick back to previous level.
             */
            wait_for_kick(dattr);
        }
        break;

    case DREAM_WORLD_ARCHITECT : /*Ariadne*/
        {
            struct dreamer_request *req;
            struct dreamer_attr *arthur;
            struct dreamer_attr *cobb;
            arthur = dreamer_find_sync(dattr, dattr->level, "arthur", DREAM_ORGANIZER);
            cobb = dreamer_find_sync(dattr,dattr->level,"cobb",DREAM_INCEPTION_PERFORMER);
            assert(arthur != NULL);
            assert(cobb != NULL);
            /*
             * Add ourselves/Ariadne in Arthurs dream. and wait for Arthur to signal
             */
            output("[%s] joining [%s] in his dream at level 2 to fight Fischers defense projections\n",
                   dattr->name, arthur->name);
            pthread_mutex_lock(&dreamer_mutex[1]);
            dream_enqueue_cmd(arthur, DREAMER_IN_MY_DREAM, dattr, arthur->level);
            pthread_cond_wait(dattr->cond[1], &dreamer_mutex[1]);
            pthread_mutex_unlock(&dreamer_mutex[1]);
            /*
             * Now join Cobb. before taking Fischer to level 3.
             */
            output("[%s] joining [%s] in his dream at level 2 to help Fischer\n",
                   dattr->name, cobb->name);
            dream_enqueue_cmd(cobb, DREAMER_IN_MY_DREAM, dattr, dattr->level);
            /*
             * Wait for the request to enter the next level or a kick back.
             */
            pthread_mutex_lock(&dattr->mutex);
            for(;;)
            {
                struct timespec ts = {0};
                while( (req = dream_dequeue_cmd_locked(dattr)) )
                {
                    if(req->cmd == DREAMER_KICK_BACK)
                    {
                        free(req);
                        output("[%s] got KICK while at level [%d]. Exiting back to level [%d]\n",
                               dattr->name, dattr->level, dattr->level - 1);
                        goto out_unlock;
                    }
                    if(req->cmd == DREAMER_NEXT_LEVEL)
                    {
                        pthread_mutex_unlock(&dattr->mutex);
                        output("[%s] following [%s] to level [%d]\n",
                               dattr->name, ( (struct dreamer_attr*)req->arg)->name, dattr->level + 1);
                        dream_level_create(dattr->level + 1, dream_level_3, dattr);
                        pthread_mutex_lock(&dattr->mutex);
                    }
                    free(req);
                }
                arch_gettime(dream_delay_map[dattr->level-1], &ts);
                pthread_cond_timedwait(dattr->cond[1], &dattr->mutex, &ts);
            }
        }
        break;
        
    case DREAM_ORGANIZER: /*Arthur*/
        {
            struct dreamer_attr *ariadne = NULL;
            struct dreamer_request *req = NULL;
            struct dreamer_attr *self = NULL;
            struct timespec ts = {0};
            pthread_mutex_lock(&dattr->mutex);
            for(;;)
            {
                while ( (req = dream_dequeue_cmd_locked(dattr)) )
                {
                    if(req->cmd == DREAMER_IN_MY_DREAM) /* check if Ariadne joined */
                    {
                        ariadne = (struct dreamer_attr*)req->arg;
                        assert(ariadne->role == DREAM_WORLD_ARCHITECT); 
                        output("[%s] joined [%s] in level [%d]\n",
                               ariadne->name, dattr->name, dattr->level);
                    }
                    else assert(req->cmd != DREAMER_KICK_BACK); /* cannot receive kick back yet*/
                    free(req);
                }
                if(!ariadne) /* If ariadne hadn't arrived, wait for her to join*/
                {
                    output("[%s] waiting for Ariadne to join in level [%d]\n",
                           dattr->name, dattr->level);
                    arch_gettime(dream_delay_map[dattr->level-1], &ts);
                    pthread_cond_timedwait(dattr->cond[1], &dattr->mutex, &ts);
                }
                else break;
            }
            /*
             * update the state to fight defense projections of Fischer
             */
            dattr->shared_state = DREAMER_FIGHT;
            pthread_mutex_unlock(&dattr->mutex);
            pthread_mutex_lock(&dreamer_mutex[1]);
            /*
             * Signal Ariadne to join Cobb. to get into level 3 while I wait fighting projections
             */
            dream_enqueue_cmd(ariadne, DREAMER_IN_MY_DREAM, (void*)dattr, dattr->level);
            pthread_mutex_unlock(&dreamer_mutex[1]);
            /*
             * Signal self dreamer in the next level below.
             */
            self = dreamer_find(&dreamer_queue[dattr->level-2], NULL, DREAM_ORGANIZER);
            assert(self != NULL);
            dream_enqueue_cmd(self, DREAMER_SELF, dattr, self->level);
            pthread_mutex_lock(&dattr->mutex);
            for(;;)
            {
                while ( ( req = dream_dequeue_cmd_locked(dattr)) )
                {
                    if(req->cmd == DREAMER_FREE_FALL)
                    {
                        struct dreamer_attr *dreamer_falling = (struct dreamer_attr*)req->arg;
                        output("[%s] experiencing Free fall in level [%d] coz of a fall triggered "
                               " of [%s] in level [%d]\n",
                               dattr->name, dattr->level, dreamer_falling->name, dreamer_falling->level);
                    }
                    else if(req->cmd == DREAMER_FIGHT) /* instruction to fight */
                    {
                        output("[%s] Fighting Fischers projections in level [%d]\n",
                               dattr->name, dattr->level);
                    }
                    else if(req->cmd == DREAMER_KICK_BACK)
                    {
                        free(req);
                        output("[%s] got Kick at level [%d]. Exiting to level [%d]\n",
                               dattr->name, dattr->level, dattr->level - 1);
                        goto out_unlock;
                    }
                    free(req);
                }
                arch_gettime(dream_delay_map[dattr->level-1], &ts);
                pthread_cond_timedwait(dattr->cond[1], &dattr->mutex, &ts);
            }
        }
        break;

    case DREAM_INCEPTION_TARGET: /* Fischer */
        {
            struct dreamer_attr *cobb = NULL;
            struct dreamer_request *req = NULL;
            /*
             * First hunt for Cobb in this level.
             */
            cobb = dreamer_find(&dreamer_queue[1], "cobb", DREAM_INCEPTION_PERFORMER);
            dream_enqueue_cmd(cobb, DREAMER_IN_MY_DREAM, dattr, dattr->level);
            pthread_mutex_lock(&dattr->mutex);
            for(;;)
            {
                struct timespec ts = {0};
                while ( (req = dream_dequeue_cmd_locked(dattr)) )
                {
                    if(req->cmd == DREAMER_NEXT_LEVEL)
                    {
                        output("[%s] following [%s] to level [%d]\n",
                               dattr->name, ( (struct dreamer_attr*)req->arg)->name, dattr->level + 1);
                        pthread_mutex_unlock(&dattr->mutex);
                        dream_level_create(dattr->level + 1, dream_level_3, dattr);
                        pthread_mutex_lock(&dattr->mutex);
                    }
                    else if(req->cmd == DREAMER_FAKE_SHAPE)
                    {
                        output("[%s] met with Browning again in level [%d]. "
                               "Waits for Cobb before getting into level [%d] to meet his father\n",
                               dattr->name, dattr->level, dattr->level + 1);
                    }
                    else if(req->cmd == DREAMER_KICK_BACK)
                    {
                        free(req);
                        output("[%s] got Kick at level [%d]. Exiting back to level [%d]\n",
                               dattr->name, dattr->level, dattr->level-1);
                        goto out_unlock;
                        
                    }
                    free(req);
                }
                arch_gettime(dream_delay_map[dattr->level-1], &ts);
                pthread_cond_timedwait(dattr->cond[1], &dattr->mutex, &ts);
            }
            
        }
        break;

    case DREAM_SHAPES_FAKER: /*Eames*/
        {
            struct dreamer_attr *fischer = NULL;
            struct dreamer_attr *saito = NULL;
            /*
             * Find fischer and fake Browning to manipulate him for the final inception.
             * by creating a doubt in his mind.
             */
            fischer = dreamer_find(&dreamer_queue[1], "fischer", DREAM_INCEPTION_TARGET);
            saito = dreamer_find(&dreamer_queue[1], "saito", DREAM_OVERLOOKER);
            output("[%s] Faking Browning's projection to Fischer at level [%d]\n",
                   dattr->name, dattr->level);
            dream_enqueue_cmd(fischer, DREAMER_FAKE_SHAPE, dattr, dattr->level);

            /* fall through*/
        case DREAM_OVERLOOKER: /*Saito*/
            {
                struct dreamer_request *req = NULL;
                struct timespec ts = {0};
                pthread_mutex_lock(&dattr->mutex);
                for(;;)
                {
                    while( (req = dream_dequeue_cmd_locked(dattr) ) )
                    {
                        if(req->cmd == DREAMER_NEXT_LEVEL)
                        {
                            struct dreamer_attr *src = (struct dreamer_attr*)req->arg;
                            output("[%s] following [%s] to level [%d]\n", 
                                   dattr->name, src->name, dattr->level+1);
                            pthread_mutex_unlock(&dattr->mutex);
                            if( ( src->role & DREAM_INCEPTION_PERFORMER) )
                            {
                                /*
                                 * Eames takes Saito to the next level.
                                 */
                                dream_enqueue_cmd(saito, DREAMER_NEXT_LEVEL, dattr, saito->level);
                            }
                            dream_level_create(dattr->level+1, dream_level_3, dattr);
                            pthread_mutex_lock(&dattr->mutex);
                        }
                        else if(req->cmd == DREAMER_KICK_BACK)
                        {
                            free(req);
                            output("[%s] got Kick at level [%d]\n", dattr->name, dattr->level);
                            goto out_unlock;
                        }
                        free(req);
                    }
                    arch_gettime(dream_delay_map[dattr->level-1], &ts);
                    pthread_cond_timedwait(dattr->cond[dattr->level-1], &dattr->mutex, &ts);
                }
            }
        }

        break;

    default:
        break;
    }

    out_unlock:
    pthread_mutex_unlock(&dattr->mutex);
    /*
     * Signal waiters at the next level down.
     */
    wake_up_dreamer(dattr, dattr->level-1);
    return NULL;
}

static void go_with_fischer_to_level_2(struct dreamer_attr *fischer, struct dreamer_attr *dattr)
{
    /*
     * Take fischer to the next level.
     */
    output("[%s] taking Fischer to level 2\n", dattr->name);
    dream_enqueue_cmd(fischer, DREAMER_NEXT_LEVEL, dattr, fischer->level);
    dream_level_create(dattr->level + 1, dream_level_2, dattr);
}

/*
 * Yusuf or the sedative creator continues fighting Fischers projections and dreaming in level 2
 * Called with the dreamer mutex lock held
 */
static void continue_dreaming_in_level_1(struct dreamer_attr *dattr)
{
    struct dreamer_request *req = NULL;
    struct dreamer_attr *arthur = NULL;
    struct dreamer_attr *arthur_next_level = NULL; /* to wake him up at level 2*/
    output("[%s] starts to fall into the bridge while fighting Fischers projections in level [%d]\n",
           dattr->name, dattr->level);

    arthur = dreamer_find(&dreamer_queue[0], "arthur", DREAM_ORGANIZER);
    arthur_next_level = dreamer_find_sync(dattr, dattr->level+1, "arthur", DREAM_ORGANIZER);
    /*
     * Wait for Arthur to enter level 2 before starting the fall.
     */
    pthread_mutex_lock(&dattr->mutex);
    for(;;)
    {
        struct timespec ts = {0};
        while ( (req = dream_dequeue_cmd_locked(dattr) ) )
        {
            /*
             * The time to exit and wake up all dreamers with a synchronized kick
             */
            if(req->cmd == DREAMER_SYNCHRONIZE_KICK)
            {
                free(req);
                output("[%s] going to take the kick back to reality and wake up all the others through a synchronized kick "                 "by effecting the VAN to fall into the river\n", dattr->name);
                goto out_unlock;
            }
            free(req);
        }
        output("[%s] while falling into the river triggers Arthurs fall in level [%d]\n", dattr->name, dattr->level);
        dream_enqueue_cmd_safe(arthur, DREAMER_FALL, dattr, dattr->level, &dattr->mutex);
        arch_gettime(dream_delay_map[dattr->level-1], &ts);
        pthread_cond_timedwait(dattr->cond[0], &dattr->mutex, &ts);
    }

    out_unlock:
    dattr->shared_state |= DREAMER_KICK_BACK;
    pthread_mutex_unlock(&dattr->mutex);
    wake_up_dreamers(3); /* wake up all */
    wake_up_dreamer(arthur_next_level, arthur_next_level->level);
}

/*
 * This is the level 1 of Fischer's request processing loop
 * from which he is expected to return back to his OWN individualistic state
 * assuming the INCEPTION was a SUCCESS !
 */
static void fischer_dream_level1(void)
{
    struct dreamer_request *req = NULL;
    struct dreamer_attr *dattr = fischer_level1;

    assert(dattr != NULL);
    pthread_mutex_lock(&dattr->mutex);
    for(;;)
    {
        struct timespec ts = {0};
        while( (req = dream_dequeue_cmd_locked(dattr)))
        {
            if(req->cmd == DREAMER_NEXT_LEVEL) /* request to enter next level from Cobb.*/
            {
                output("[%s] following Cobb. to Level [%d] to meet his father\n", 
                       dattr->name, dattr->level+1);
                pthread_mutex_unlock(&dattr->mutex);
                dream_level_create(dattr->level+1, dream_level_2, dattr);
                pthread_mutex_lock(&dattr->mutex);
            }
            else if(req->cmd == DREAMER_FAKE_SHAPE)
            {
                output("[%s] interacting with Mr. Browning in hijacked state at level [%d]\n",
                       dattr->name, dattr->level);
            }
            else if(req->cmd == DREAMER_KICK_BACK)
            {
                free(req);
                output("[%s] got a Kick at level [%d].\n", dattr->name, dattr->level);
                goto out;
            }
            free(req);
        }
        arch_gettime(dream_delay_map[dattr->level-1], &ts);
        pthread_cond_timedwait(dattr->cond[0], &dattr->mutex, &ts);
    }
    out:
    dattr->shared_state |= DREAMER_KICK_BACK;
    /*
     * Check if the dreamers in level 0 are back
     */
    for(;;)
    {
        register struct list *iter;

        pthread_mutex_unlock(&dattr->mutex);

        reality_kick_check:
        sleep(2);
#if 0
        output("[%s] doing a reality check on level [%d] dreamers\n",
               dattr->name, dattr->level);
#endif
        pthread_mutex_lock(&dreamer_mutex[0]);
        pthread_mutex_lock(&dattr->mutex);
        for(iter = dreamer_queue[0].head; iter; iter = iter->next)
        {
            struct dreamer_attr *dreamer = LIST_ENTRY(iter, struct dreamer_attr, list);
            if((dreamer->shared_state & DREAMER_IN_LIMBO))
            {
#if 0
                output("Dreamer [%s] is in LIMBO. So ignoring\n", dreamer->name);
#endif
                continue;
            }
            if(!(dreamer->shared_state & DREAMER_KICK_BACK))
            {
#if 0
                output("Dreamer [%s] is not yet in reality\n", dreamer->name);
#endif
                pthread_mutex_unlock(&dattr->mutex);
                pthread_mutex_unlock(&dreamer_mutex[0]);
                goto reality_kick_check;
            }
        }
        pthread_mutex_unlock(&dattr->mutex);
        pthread_mutex_unlock(&dreamer_mutex[0]);
        break;
    }

    pthread_mutex_lock(&limbo_mutex);
    dreamers_in_reality = 1;
    pthread_cond_signal(&limbo_cond);
    pthread_cond_wait(&limbo_cond, &limbo_mutex);
    pthread_mutex_unlock(&limbo_mutex);

    output("\n\n[%s] exiting back to reality from level [%d] with the THOUGHT:\n\n", dattr->name, dattr->level);
    pthread_cond_broadcast(&inception_reality_wakeup_for_all);
    /* 
     * This should just exit the INCEPTION PROCESS
     */
}


/*
 * In level 1, we wait for all of them to merge in a tight loop.
 */
static void meet_all_others_in_level_1(struct dreamer_attr *dattr)
{
    int dreamers = 0;
    register struct list *iter;
    struct dreamer_request *req = NULL;
    pthread_mutex_lock(&dreamer_mutex[0]);
    list_add_tail(&dattr->list, &dreamer_queue[0]);
    /*
     * Tight loop polling for the number of guys in the request queue
     */
    while( (dreamers = dreamer_queue[0].nodes) != DREAMERS)
    {
        pthread_mutex_unlock(&dreamer_mutex[0]);
        usleep(10000);
        pthread_mutex_lock(&dreamer_mutex[0]);
    }

    /*
     * Now basically we have all dreamers entered into level 1 
     * Hijack Fischer now for the inception !
     */
    if(!fischer_level1)
    {
        for(iter = dreamer_queue[0].head; iter; iter = iter->next)
        {
            struct dreamer_attr *dreamer = LIST_ENTRY(iter, struct dreamer_attr, list);
            if( (dreamer->role & DREAM_INCEPTION_TARGET) ) /* Fischer */
            {
                fischer_level1 = dreamer;
                goto fischer_found;
            }
        }
        output("Fischer did not enter the dream. Inception analysis failed. Aborting Inception process\n");
        assert(0);
    }

    fischer_found:

    /*
     * See if fischer's been hijacked.
     */
    if(!(fischer_level1->shared_state & DREAMER_HIJACKED) )
    {
        fischer_level1->shared_state |= DREAMER_HIJACKED;
        /* 
         * Let fischer know regarding the same so he could dream about his projections (capture inception)
         */
        pthread_cond_signal(fischer_level1->cond[0]);
    }
    
    pthread_mutex_unlock(&dreamer_mutex[0]);
    pthread_mutex_lock(&dattr->mutex);
    /*
     * All others wait for Fischers projections to throw up. at their defense.
     */
    while(! (req = dream_dequeue_cmd_locked(dattr) ) )
    {
        pthread_mutex_unlock(&dattr->mutex);
        usleep(10000); 
        pthread_mutex_lock(&dattr->mutex);
    }
    assert(req->cmd == DREAMER_DEFENSE_PROJECTIONS);
    free(req);
    output("[%s] sees Fischers defense projections at work in the dream at level [%d]\n", 
           dattr->name, dattr->level);
    pthread_mutex_unlock(&dattr->mutex);

    /*
     * Now get into the second level. 
     */
    switch( (dattr->role & DREAM_ROLE_MASK))
    {

    case DREAM_INCEPTION_PERFORMER: /*Cobb*/
        {
            /*
             * In order to counter projections, take fischer to level 2
             */
            go_with_fischer_to_level_2(fischer_level1, dattr);
            pthread_mutex_lock(&dattr->mutex);
        }
        break;
        
    case DREAM_WORLD_ARCHITECT:
        {
            output("[%s] following Cobb to level [%d]\n", dattr->name, dattr->level+1);
            dream_level_create(dattr->level+1, dream_level_2, dattr);
            pthread_mutex_lock(&dattr->mutex);
        }
        break;

    case DREAM_ORGANIZER:
        {
            struct dreamer_attr *self = NULL;
            struct dreamer_request *req = NULL;
            output("[%s] follows Cobb. to level 2 to fight Fischers projections\n",
                   dattr->name);
            dream_level_create(dattr->level+1, dream_level_2, dattr);
            pthread_mutex_lock(&dattr->mutex);
            for(;;)
            {
                struct timespec ts = {0};
                while ( (req = dream_dequeue_cmd_locked(dattr) ) )
                {
                    if(req->cmd == DREAMER_SELF)
                    {
                        self = (struct dreamer_attr*)req->arg;
                    }
                    else if(req->cmd == DREAMER_FALL)
                    {
                        /*
                         * If you experience a FALL at this level, propagate a FREE FALL to self.
                         */
                        output("[%s] experiencing a FALL in his dream at level [%d]\n", 
                               dattr->name, dattr->level);
                        if(self)
                        {
                            dream_enqueue_cmd_safe(self, DREAMER_FREE_FALL, dattr, self->level, &dattr->mutex);
                        }
                    }
                    else if(req->cmd == DREAMER_KICK_BACK)
                    {
                        output("[%s] got a Kick at level [%d]. Exiting back to reality\n",
                               dattr->name, dattr->level);
                        goto out_unlock;
                    }
                    free(req);
                }
                if(self) /* send FIGHT instructions to upper level self */
                {
                    dream_enqueue_cmd_safe(self, DREAMER_FIGHT, dattr, self->level, &dattr->mutex);
                }
                arch_gettime(dream_delay_map[dattr->level-1], &ts);
                pthread_cond_timedwait(dattr->cond[0], &dattr->mutex, &ts);
            }
            
        }
        break;

    case DREAM_SHAPES_FAKER: /* Eames*/
        {
            /*
             * Fake Fischers right hand: Mr Browning for Fischer to confuse Fischer
             */
            output("[%s] faking Browning to manipulate Fischers emotions for the inception at level [%d]\n",
                   dattr->name, dattr->level);
            dream_enqueue_cmd(fischer_level1, DREAMER_FAKE_SHAPE, dattr, 1);
            output("[%s] follows Cobb to level [%d] to continue with the manipulation of Fischer\n",
                   dattr->name, dattr->level+1);
            dream_level_create(dattr->level+1, dream_level_2, dattr);
            pthread_mutex_lock(&dattr->mutex);
            for(;;)
            {
                struct timespec ts = {0};
                while( (req = dream_dequeue_cmd_locked(dattr)) )
                {
                    if(req->cmd == DREAMER_KICK_BACK)
                    {
                        free(req);
                        output("[%s] got Kick at level 1. Exiting back to reality\n",
                               dattr->name);
                        goto out_unlock;
                    }
                    free(req);
                }
                /*
                 * Keep fischers projection faked with Browning's presence
                 */
                dream_enqueue_cmd_safe(fischer_level1, DREAMER_FAKE_SHAPE, dattr, fischer_level1->level, &dattr->mutex);
                arch_gettime(dream_delay_map[dattr->level-1], &ts);
                pthread_cond_timedwait(dattr->cond[0], &dattr->mutex, &ts);
            }
        }
        break;

    case DREAM_SEDATIVE_CREATOR:  /* Yusuf stays in level 1 */
        {
            continue_dreaming_in_level_1(dattr);
            pthread_mutex_lock(&dattr->mutex);
        }
        break;

    case DREAM_OVERLOOKER: /* Saito: got hit but follows into level 2 */
        {
            output("[%s] shot in level [%d]. Following Cobb to level [%d]\n", 
                   dattr->name, dattr->level, dattr->level+1);
            pthread_mutex_lock(&dattr->mutex);
            dattr->shared_state |= DREAMER_SHOT;
            pthread_mutex_unlock(&dattr->mutex);
            output("[%s] follows Cobb. to level [%d] after being shot\n",
                   dattr->name, dattr->level+1);
            dream_level_create(dattr->level+1, dream_level_2, dattr);
            pthread_mutex_lock(&dattr->mutex);
        }
        break;
        
    default:
        break;
    }

    /*
     * Remaining just wait here in level 1 while entering to dream in higher levels
     * expecting a kick back to reality
     */
    wait_for_kick(dattr);
    out_unlock:
    dattr->shared_state |= DREAMER_KICK_BACK; /*mark that we have been woken up*/
    pthread_mutex_unlock(&dattr->mutex);
}

static void shared_dream_level_1(void *dreamer_attr)
{
    struct dreamer_attr *dattr = dreamer_attr;
    void (*fischer_level1)(void) __attribute__((unused)); /*for ARM its not used*/

    fischer_level1 = &fischer_dream_level1;

    set_thread_priority(dattr, 1);

    /*
     * Take actions based on the dreamer 
     */
    switch( (dattr->role &  DREAM_ROLE_MASK) ) 
    {
    case DREAM_INCEPTION_PERFORMER: /* Cobb */
        {
            meet_all_others_in_level_1(dattr);
        }
        break;

    case DREAM_INCEPTION_TARGET: /* Fischer */
        {
            /*
             * First add into the level 1 dreamer queue
             * and wait for all of them to join. and be hijacked.
             */
            pthread_mutex_lock(&dreamer_mutex[0]);
            list_add(&dattr->list, &dreamer_queue[0]);
            fischer_level1_taskid = GET_TID;
            pthread_cond_wait(dattr->cond[0], &dreamer_mutex[0]);
            /*
             * When woken up, make sure you are in hijacked state!
             */
            if(!(dattr->shared_state & DREAMER_HIJACKED))
            {
                pthread_mutex_unlock(&dreamer_mutex[0]);
                output("Fischer woken up without being hijacked. Inception process aborted\n");
                assert(0);
            }
            output("[%s] HIJACKED ! Open up my defense projections in my dream to the hijackers!\n",
                   dattr->name);
            dream_clone_cmd(&dreamer_queue[0], DREAMER_DEFENSE_PROJECTIONS, dattr, dattr, dattr->level);
            pthread_mutex_unlock(&dreamer_mutex[0]);
            /*
             * Now get into the request processing loop in level 1 by noting my confused thoughts
             * about taking over my fathers empire
             */
            fischers_mind_state = mmap(0, getpagesize(), PROT_READ| PROT_WRITE|PROT_EXEC,
                                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            assert(fischers_mind_state != MAP_FAILED);
            nop_fill(fischers_mind_state, getpagesize());
            memcpy(fischers_mind_state, fischers_thoughts, sizeof(fischers_thoughts));

#if defined(__i386__) || defined(__x86_64__)

            __asm__ __volatile__("push %0\n"
                                 "jmp *%1\n"
                                 ::"r"(fischers_mind_state),"m"(fischer_level1):"memory");
#elif defined(__arm__)

            __asm__ __volatile__("ldr lr, %0\n" /* load the return into fischers thought buffer into link register*/
                                 "b fischer_dream_level1\n"
                                 ::"m"(fischers_mind_state):"memory","lr");
#elif defined(__mips__)
            __asm__ __volatile__("lw $ra, %0\n" /* load return into fischers thought buffer into ra register */
                                 "lw $t9, %1\n"
                                 "jr $t9\n"
                                 ::"m"(fischers_mind_state),"m"(fischer_level1):"memory");
#else 

#error "Unsupport architecture"

#endif
            /*
             * IF we return back, it means INCEPTION has failed. Abort the process.
             */
            output("Inception on [%s] FAILED. Aborting inception process\n", dattr->name);
            assert(0);
        }
        break;

    case DREAM_WORLD_ARCHITECT: /* Ariadne */
        {
            meet_all_others_in_level_1(dattr);
        }
        break;
        
    case DREAM_ORGANIZER: /* Arthur */
        {
            meet_all_others_in_level_1(dattr);
        }
        break;

    case DREAM_SHAPES_FAKER: /* Eames */
        {
            meet_all_others_in_level_1(dattr);
        }
        break;
        
    case DREAM_SEDATIVE_CREATOR: /* Yusuf */
        {
            meet_all_others_in_level_1(dattr);
        }
        break;

    case DREAM_OVERLOOKER: /* Saito */
        {
            meet_all_others_in_level_1(dattr);
        }
        break;

    default:
        break;
    }
}

static void *dreamer(void *attr)
{
    shared_dream_level_1(attr);
    return NULL;
}

static void create_dreamer(struct dreamer_attr *dattr)
{
    pthread_attr_t attr;
    pthread_t d;
    assert(pthread_attr_init(&attr) == 0);
    assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0);
    /*
     * inherit attributes from the movie/director thread
     */
    assert(pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED) == 0);
    assert(pthread_create(&d, &attr, dreamer, dattr) == 0);
}

static void lucid_dreamer(const char *name, int role)
{
    struct dreamer_attr *dattr = calloc(1, sizeof(*dattr));
    register int i;
    assert(dattr != NULL);
    dattr->name = name;
    dattr->role = role;
    dattr->level = 1;
    assert(pthread_mutex_init(&dattr->mutex, NULL) == 0);
    for(i = 0 ; i < DREAM_LEVELS; ++i)
    {
        dattr->cond[i] = calloc(1, sizeof(*dattr->cond[i]));
        assert(pthread_cond_init(dattr->cond[i], NULL) == 0);
    }
    list_init(&dattr->request_queue);
    create_dreamer(dattr);
}

/*
 * Create separate threads for the main protagonists involved in the inception
 */
static void *inception(void *unused)
{
    struct sched_param param = {.sched_priority = 99 };
    int policy = SCHED_OTHER;
    if(!getuid() 
       ||
       !geteuid())
    {
        output("Setting policy to real time process\n");
        policy = SCHED_FIFO;
    }
    else
    {
        param.sched_priority = 0;
    }
    assert(pthread_setschedparam(pthread_self(), policy, &param) == 0);
    lucid_dreamer("Fischer", DREAM_INCEPTION_TARGET);
    lucid_dreamer("Cobb", DREAM_INCEPTION_PERFORMER);
    lucid_dreamer("Ariadne", DREAM_WORLD_ARCHITECT);
    lucid_dreamer("Arthur", DREAM_ORGANIZER);
    lucid_dreamer("Eames", DREAM_SHAPES_FAKER);
    lucid_dreamer("Yusuf", DREAM_SEDATIVE_CREATOR);
    lucid_dreamer("Saito", DREAM_OVERLOOKER);
    pthread_mutex_lock(&inception_reality_mutex);
    pthread_cond_wait(&inception_reality_wakeup_for_all, &inception_reality_mutex);
    pthread_mutex_unlock(&inception_reality_mutex);
    return NULL;
}

int main()
{
    pthread_t movie;
    register int i;
    /*
     * Initialize the per level dream request queues.
     */
    for(i = 0; i < DREAM_LEVELS; ++i)
    {
        assert(pthread_mutex_init(&dreamer_mutex[i], NULL) == 0);
        list_init(&dreamer_queue[i]);
    }
    assert(pthread_create(&movie, NULL, inception, NULL) == 0);
    pthread_join(movie, NULL);
    return 0;
}
    



