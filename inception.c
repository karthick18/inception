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
#ifndef __i386__
#define __i386__
#define __fake_x86__
#endif
#include <asm/unistd.h>
#ifdef __fake_x86__
#undef __i386__
#undef __fake_x86__
#endif
#include <pthread.h>
#include <assert.h>
#include <sched.h>
#include <sys/mman.h>
#include <time.h>
#include <syscall.h>
#include "list.h"

#define DREAM_INCEPTION_TARGET 0x1
#define DREAM_INCEPTION_PERFORMER 0x2
#define DREAM_WORLD_ARCHITECT 0x4
#define DREAM_ORGANIZER 0x8
#define DREAM_SHAPES_FAKER 0x10
#define DREAM_SEDATIVE_CREATOR 0x20
#define DREAM_OVERLOOKER 0x40
#define DREAM_ROLE_MASK 0x7f
#define DREAMERS (0x7)

#define output(fmt, arg...) do { fprintf(stderr, fmt, ##arg); } while(0)

struct inception_definitions
{
    const char *term;
    const char *definition;
} inception_definitions[] = {
    {.term= "limbo",  .definition = "State of infinite subconsciousness" },
    {.term = "totem", .definition = "Object to determine if one is in a dream world or reality" },
};

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
#include "inception.h"

static char *fischers_mind_state;
static struct dreamer_attr *fischer_level1;
static pid_t fischer_level1_taskid; /*fischers level1 taskid*/
/*
 * Queue the command to the dreamers request queue
 */
static __inline__ void dream_enqueue_cmd(struct dreamer_attr *dattr, int cmd, void *arg, int level)
{
    struct dreamer_request *req = calloc(1, sizeof(*req));
    assert(req != NULL);
    assert(level > 0 && level <= DREAM_LEVELS);
    req->dattr = dattr;
    req->cmd = cmd;
    req->arg = arg;
    pthread_mutex_lock(&dattr->mutex);
    list_add_tail(&req->list, &dattr->request_queue); 
    pthread_cond_signal(dattr->cond[level-1]);
    pthread_mutex_unlock(&dattr->mutex);
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

static __inline__ struct dreamer_request *dream_dequeue_cmd(struct dreamer_attr *dattr)
{
    struct dreamer_request *req = NULL;
    struct list *head = NULL;
    pthread_mutex_lock(&dattr->mutex);
    if(!dattr->request_queue.nodes) 
    {
        pthread_mutex_unlock(&dattr->mutex);
        return NULL;
    }
    head = dattr->request_queue.head;
    assert(head != NULL);
    req = LIST_ENTRY(head, struct dreamer_request, list);
    list_del(head, &dattr->request_queue);
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
 * Level 0 is wake up dreamers on all levels
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
            dream_enqueue_cmd(dattr, DREAMER_KICK_BACK, NULL, dattr->level);
        }
        pthread_mutex_unlock(&dreamer_mutex[i]);
    }
}

/*
 * Called with the dreamer mutex held for the appropriate level.
 */
static void wait_for_kick(struct dreamer_attr *dattr)
{
    struct dreamer_request *req = NULL;
    int sleep_time = dattr->level * 2;
    for(;;)
    {
        struct timespec ts = {0};
        while ( (req = dream_dequeue_cmd(dattr)) )
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
        assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
        ts.tv_sec += sleep_time;
        pthread_cond_timedwait(dattr->cond[dattr->level-1], &dreamer_mutex[dattr->level-1], &ts);
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
 * Limbo is a state of infinite subconciousness
 */
static void enter_limbo(struct dreamer_attr *dattr)
{
    struct dreamer_attr *clone = NULL;
    struct dreamer_request *req = NULL;
    for(;;) sleep(10);
    dattr->shared_state |= DREAMER_IN_LIMBO;
    clone = dream_attr_clone(dattr->level+1, dattr);
    assert(clone != NULL);
    pthread_mutex_lock(&dreamer_mutex[3]);
    list_add_tail(&clone->list, &dreamer_queue[3]);
    while( (dreamer_queue[3].nodes + 3) != DREAMERS)
    {
        pthread_mutex_unlock(&dreamer_mutex[3]);
        usleep(100000);
        pthread_mutex_lock(&dreamer_mutex[3]);
    }

    switch( (clone->role & DREAM_ROLE_MASK) )
    {
    case DREAM_INCEPTION_PERFORMER: /* Cobb */
        {

        }
        break;

    case DREAM_WORLD_ARCHITECT: /* Ariadne */
        {

        }
        break;

    case DREAM_OVERLOOKER: /* Saito */
        {

        }
        break;

    case DREAM_INCEPTION_TARGET: /*Fischer*/
        {

        }
        break;
    }
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
            /*
             * Self enqueue
             */
            fischer = dreamer_find(&dreamer_queue[2], "fischer", DREAM_INCEPTION_TARGET);
            ariadne = dreamer_find(&dreamer_queue[2], "ariadne", DREAM_WORLD_ARCHITECT);
            eames = dreamer_find(&dreamer_queue[2], "eames", DREAM_SHAPES_FAKER);
            dream_enqueue_cmd(dattr, DREAMER_FIGHT, NULL, dattr->level);
            dream_enqueue_cmd(dattr, DREAMER_IN_MY_DREAM, (void*)"Mal", dattr->level);
            for(;;)
            {
                while ( (req = dream_dequeue_cmd(dattr)))
                {
                    if(req->cmd == DREAMER_FIGHT)
                    {
                        output("[%s] fights Fischers defense projections in level [%d]\n",
                               dattr->name, dattr->level);
                    }
                    else if(req->cmd == DREAMER_IN_MY_DREAM)
                    {
                        output("[%s] sees his wife [%s] in his dream. [%s] shoots Fischer\n",
                               dattr->name, (char*)req->arg, (char*)req->arg);
                        dream_enqueue_cmd(fischer, DREAMER_SHOT, (void*)"Mal", fischer->level);
                        /*
                         * Let Eames know so he could start recovery on Fischer
                         */
                        dream_enqueue_cmd(eames, DREAMER_SHOT, fischer, dattr->level);
                        /*
                         * Ask Ariadne to follow me in limbo.
                         */
                        output("[%s] asks [%s] to follow him in limbo with his wife\n",
                               dattr->name, ariadne->name);
                        dream_enqueue_cmd(ariadne, DREAMER_IN_MY_DREAM, dattr, dattr->level);
                        /*
                         * Give up the lock and take a break before trying to grab the lock again
                         */
                        pthread_mutex_unlock(&dreamer_mutex[2]);
                        usleep(100000);
                        pthread_mutex_lock(&dreamer_mutex[2]);
                        /*
                         * Self enqueue to get into limbo.
                         */
                        dream_enqueue_cmd(dattr, DREAMER_IN_LIMBO, dattr, dattr->level);
                    }
                    else if(req->cmd == DREAMER_IN_LIMBO)
                    {
                        output("[%s] enters limbo with his Wifes projections in level [%d]\n",
                               dattr->name, dattr->level);
                        pthread_mutex_unlock(&dreamer_mutex[2]);
                        enter_limbo(dattr);
                        pthread_mutex_lock(&dreamer_mutex[2]);
                        /*
                         * should not be reached
                         */
                        output("[%s] returned from limbo. Exiting out at level [%d]\n", dattr->name, 
                               dattr->level);
                        exit(0);
                    }
                    free(req);
                }
                pthread_mutex_unlock(&dreamer_mutex[2]);
                sleep(2);
                pthread_mutex_lock(&dreamer_mutex[2]);
            }
        }
        break;

    case DREAM_WORLD_ARCHITECT: /* Ariadne*/
        {
            /*
             * Wait for Cobbs command to enter his dream in limbo with him.
             */
            struct timespec ts = {0};
            int ret_from_limbo = 0;
            for(;;)
            {
                while ( (req = dream_dequeue_cmd(dattr)) )
                {
                    if(req->cmd == DREAMER_IN_MY_DREAM)
                    {
                        output("[%s] enters with Cobb. to meet his wife in Limbo at level [%d]\n",
                               dattr->name, dattr->level);
                        pthread_mutex_unlock(&dreamer_mutex[2]);
                        enter_limbo(dattr);
                        pthread_mutex_lock(&dreamer_mutex[2]);
                    }
                    else if(req->cmd == DREAMER_KICK_BACK)
                    {
                        if(!ret_from_limbo)
                        {
                            output("[%s] returned from Fischers limbo to level [%d]\n", 
                                   dattr->name, dattr->level);
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
                assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
                ts.tv_sec += 6;
                pthread_cond_timedwait(dattr->cond[2], &dreamer_mutex[2], &ts);
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
            dream_enqueue_cmd(saito, DREAMER_KILLED, saito, dattr->level);
            for(;;)
            {
                while ( (req = dream_dequeue_cmd(dattr)))
                {
                    if(req->cmd == DREAMER_FIGHT)
                    {
                        output("[%s] fights Fischers defense projections at level [%d]\n",
                               dattr->name, dattr->level);
                    }
                    else if(req->cmd == DREAMER_SHOT)
                    {
                        fischer = ( (struct dreamer_attr*)req->arg);
                        output("[%s] sees [%s] shot in level [%d]. Starts recovery\n",
                               dattr->name, fischer->name, dattr->level);
                        dream_enqueue_cmd(dattr, DREAMER_RECOVER, fischer, dattr->level);
                    }
                    else if(req->cmd == DREAMER_RECOVER)
                    {
                        output("[%s] doing recovery on [%s] who is shot at level [%d]\n",
                               dattr->name, ( (struct dreamer_attr*)req->arg)->name, dattr->level);
                    }
                    else if(req->cmd == DREAMER_KICK_BACK)
                    {
                        free(req);
                        output("[%s] got Kick at level [%d]. Exiting back to level [%d]\n",
                               dattr->name, dattr->level, dattr->level - 1);
                        goto out_unlock;
                    }
                    free(req);
                }
                if(fischer)
                {
                    /*
                     * Keep recovering fischer if he is shot in this level.
                     */
                    dream_enqueue_cmd(dattr, DREAMER_RECOVER, fischer, dattr->level);
                }
                assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
                ts.tv_sec += 6;
                pthread_cond_timedwait(dattr->cond[2], &dreamer_mutex[2], &ts);
            }
        }
        break;

    case DREAM_OVERLOOKER: /*Saito*/
        {
            struct timespec ts = {0};
            for(;;)
            {
                while( (req = dream_dequeue_cmd(dattr)))
                {
                    if(req->cmd == DREAMER_FIGHT)
                    {
                        output("[%s] fights Fischers projections in level [%d]\n", 
                               dattr->name, dattr->level);
                    }
                    else if(req->cmd == DREAMER_KILLED) /* Killed. Enter limbo */
                    {
                        output("[%s] gets killed at level [%d]. Enters limbo\n", dattr->name, dattr->level);
                        pthread_mutex_unlock(&dreamer_mutex[2]);
                        enter_limbo(dattr);
                        pthread_mutex_lock(&dreamer_mutex[2]);
                        /*
                         * Unreached.
                         */
                        output("[%s] returned back from Limbo at level [%d]\n", dattr->name, dattr->level);
                        exit(0);
                    }
                    free(req);
                }
                assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
                ts.tv_sec += 6;
                pthread_cond_timedwait(dattr->cond[2], &dreamer_mutex[2], &ts);
            }
        }
        break;

    case DREAM_INCEPTION_TARGET: /* Fischer */
        {
            int reconciled = 0;
            struct timespec ts = {0};
            for(;;)
            {
                while ( (req = dream_dequeue_cmd(dattr)))
                {
                    if(req->cmd == DREAMER_SHOT)
                    {
                        output("[%s] shot by [%s] in level [%d]\n", 
                               dattr->name, (char*)req->arg, dattr->level);
                        /*
                         * Freeze for sometime before joining Cobb and Ariadne in limbo.
                         */
                        pthread_mutex_unlock(&dreamer_mutex[2]);
                        usleep(100000);
                        enter_limbo(dattr);
                        pthread_mutex_lock(&dreamer_mutex[2]);
                    }
                    else if(req->cmd == DREAMER_KICK_BACK)
                    {
                        if(!reconciled)
                        {
                            struct dreamer_attr *cobb = NULL;
                            output("[%s] got a kick back from Limbo to reconcile with his father at level [%d]\n",
                                   dattr->name, dattr->level);
                            reconciled = 1;
                            cobb = dreamer_find(&dreamer_queue[2], "cobb", DREAM_INCEPTION_PERFORMER);
                            dream_enqueue_cmd(cobb, DREAMER_RECOVER, dattr, cobb->level);
                        }
                        else 
                        {
                            free(req);
                            output("[%s] got a kick at level [%d]. Falling back to level [%d]\n",
                                   dattr->name, dattr->level, dattr->level - 1);
                            goto out_unlock;
                        }
                    }
                    free(req);
                }
                assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
                ts.tv_sec += 6;
                pthread_cond_timedwait(dattr->cond[2], &dreamer_mutex[2], &ts);
            }
        }
        break;
        
    default:
        break;
    }
    out_unlock:
    pthread_mutex_unlock(&dreamer_mutex[2]);
    wake_up_dreamers(2);
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
    switch((dattr->role & DREAM_ROLE_MASK))
    {
    case DREAM_INCEPTION_PERFORMER: /* Cobb in level 2 */
        {
            /*
             * Wait for Ariadne and Fischer to join me. at this level after meeting with Arthur
             */
            struct dreamer_request *req;
            int wait_for_dreamers = DREAM_WORLD_ARCHITECT | DREAM_INCEPTION_TARGET;
            while(wait_for_dreamers > 0)
            {
                while( (!(req = dream_dequeue_cmd(dattr)) ) 
                       ||
                       req->cmd != DREAMER_IN_MY_DREAM 
                       ||
                       !req->arg
                       ||
                       !( ((struct dreamer_attr*)req->arg)->role & wait_for_dreamers)
                       )
                {
                    if(req) free(req);
                    pthread_mutex_unlock(&dreamer_mutex[1]);
                    usleep(10000);
                    pthread_mutex_lock(&dreamer_mutex[1]);
                }
                wait_for_dreamers &= ~((struct dreamer_attr*)req->arg)->role;
                output("[%s] taking [%s] to level 3\n", dattr->name, ((struct dreamer_attr*)req->arg)->name);
                dream_enqueue_cmd((struct dreamer_attr*)req->arg, DREAMER_NEXT_LEVEL, dattr, dattr->level);
                free(req);
            }
            /*
             * Ariadne + Fischer has joined. Go to level 3. myself
             */
            dream_level_create(dattr->level + 1, dream_level_3, dattr);
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
            arthur = dreamer_find_sync_locked(dattr, dattr->level, "arthur", DREAM_ORGANIZER);
            cobb = dreamer_find_sync_locked(dattr,dattr->level,"cobb",DREAM_INCEPTION_PERFORMER);
            assert(arthur != NULL);
            assert(cobb != NULL);
            /*
             * Add ourselves/Ariadne in Arthurs dream. and wait for Arthur to signal
             */
            output("[%s] joining [%s] in his dream at level 2 to fight Fischers defense projections\n",
                   dattr->name, arthur->name);
            dream_enqueue_cmd(arthur, DREAMER_IN_MY_DREAM, dattr, dattr->level);
            pthread_cond_wait(dattr->cond[1], &dreamer_mutex[1]);
            /*
             * Now join Cobb. before taking Fischer to level 3.
             */
            output("[%s] joining [%s] in his dream at level 2 to help Fischer\n",
                   dattr->name, cobb->name);
            dream_enqueue_cmd(cobb, DREAMER_IN_MY_DREAM, dattr, dattr->level);
            /*
             * Wait for the request to enter the next level or a kick back.
             */
            for(;;)
            {
                struct timespec ts = {0};
                while( (req = dream_dequeue_cmd(dattr)) )
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
                        output("[%s] following [%s] to level [%d]\n",
                               dattr->name, ( (struct dreamer_attr*)req->arg)->name, dattr->level + 1);
                        dream_level_create(dattr->level + 1, dream_level_3, dattr);
                    }
                    free(req);
                }
                assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
                ts.tv_sec += 4;
                pthread_cond_timedwait(dattr->cond[1], &dreamer_mutex[1], &ts);
            }
        }
        break;
        
    case DREAM_ORGANIZER: /*Arthur*/
        {
            struct dreamer_attr *ariadne = NULL;
            struct dreamer_request *req = NULL;
            struct dreamer_attr *self = NULL;
            struct timespec ts = {0};
            for(;;)
            {
                while ( (req = dream_dequeue_cmd(dattr)) )
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
                    assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
                    ts.tv_sec += 4;
                    pthread_cond_timedwait(dattr->cond[1], &dreamer_mutex[1], &ts);
                }
                else break;
            }
            /*
             * update the state to fight defense projections of Fischer
             */
            dattr->shared_state = DREAMER_FIGHT;
            /*
             * Signal Ariadne to join Cobb. to get into level 3 while I wait fighting projections
             */
            dream_enqueue_cmd(ariadne, DREAMER_IN_MY_DREAM, (void*)dattr, dattr->level);
            /*
             * Signal self dreamer in the next level below.
             */
            self = dreamer_find(&dreamer_queue[dattr->level-2], NULL, DREAM_ORGANIZER);
            assert(self != NULL);
            dream_enqueue_cmd(self, DREAMER_SELF, dattr, self->level);
            for(;;)
            {
                while ( ( req = dream_dequeue_cmd(dattr)) )
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
                assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
                ts.tv_sec += 4;
                pthread_cond_timedwait(dattr->cond[1], &dreamer_mutex[1], &ts);
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
            for(;;)
            {
                struct timespec ts = {0};
                while ( (req = dream_dequeue_cmd(dattr)) )
                {
                    if(req->cmd == DREAMER_NEXT_LEVEL)
                    {
                        output("[%s] following [%s] to level [%d]\n",
                               dattr->name, ( (struct dreamer_attr*)req->arg)->name, dattr->level + 1);
                        dream_level_create(dattr->level + 1, dream_level_3, dattr);
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
                assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
                ts.tv_sec += 4;
                pthread_cond_timedwait(dattr->cond[1], &dreamer_mutex[1], &ts);
            }
            
        }
        break;

    case DREAM_SHAPES_FAKER: /*Eames*/
        {
            struct dreamer_attr *fischer = NULL;
            /*
             * Find fischer and fake Browning to manipulate him for the final inception.
             * by creating a doubt in his mind.
             */
            fischer = dreamer_find(&dreamer_queue[1], "fischer", DREAM_INCEPTION_TARGET);
            output("[%s] Faking Browning's projection to Fischer at level [%d]\n",
                   dattr->name, dattr->level);
            dream_enqueue_cmd(fischer, DREAMER_FAKE_SHAPE, dattr, dattr->level);
            /*
             * Follow cobb to level 3
             */
            output("[%s] following Cobb to level [%d]\n", dattr->name, dattr->level+1);
            dream_level_create(dattr->level + 1, dream_level_3, dattr);
            wait_for_kick(dattr);
        }
        break;

    case DREAM_OVERLOOKER: /*Saito*/
        {
            /*
             * Follow Cobb to level 3. Also you are shot. in level 1
             */
            output("[%s] following Cobb to level [%d]\n", dattr->name, dattr->level+1);
            dream_level_create(dattr->level + 1, dream_level_3, dattr);
            wait_for_kick(dattr);
        }
        break;

    default:
        break;
    }

    out_unlock:
    pthread_mutex_unlock(&dreamer_mutex[1]);
    /*
     * Signal waiters at level 1
     */
    wake_up_dreamers(1);
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
    output("[%s] starts to fall into the bridge while fighting Fischers projections in level [%d]\n",
           dattr->name, dattr->level);

    arthur = dreamer_find(&dreamer_queue[0], "arthur", DREAM_ORGANIZER);
    pthread_mutex_unlock(&dreamer_mutex[0]);
    /*
     * Wait for Arthur to enter level 2 before starting the fall.
     */
    pthread_mutex_lock(&dreamer_mutex[0]);
    for(;;)
    {
        struct timespec ts = {0};
        while ( (req = dream_dequeue_cmd(dattr) ) )
        {
            /*
             * The time to exit and wake up all dreamers
             */
            if(req->cmd == DREAMER_SYNCHRONIZE_KICK)
            {
                free(req);
                output("[%s] going to wake up all the others through a synchronized kick by effecting the VAN "
                       "to fall into the bridge\n", dattr->name);
                goto out_unlock;
            }
            free(req);
        }
        output("[%s] while falling into the bridge trigger Arthurs fall in level [%d]\n", dattr->name, dattr->level);
        dream_enqueue_cmd(arthur, DREAMER_FALL, dattr, dattr->level);
        assert(clock_gettime(CLOCK_REALTIME, &ts) ==0);
        ts.tv_sec += 2;
        pthread_cond_timedwait(dattr->cond[0], &dreamer_mutex[0], &ts);
    }

    out_unlock:
    pthread_mutex_unlock(&dreamer_mutex[0]);
    wake_up_dreamers(0); /* wake up all */
}

/*
 * This is the level 1 of Fischer's request processing loop
 */
static void fischer_dream_level1(void)
{
    struct dreamer_request *req = NULL;
    struct dreamer_attr *dattr = fischer_level1;

    assert(dattr != NULL);
    pthread_mutex_lock(&dreamer_mutex[0]);
    for(;;)
    {
        struct timespec ts = {0};
        while( (req = dream_dequeue_cmd(dattr)))
        {
            if(req->cmd == DREAMER_NEXT_LEVEL) /* request to enter next level from Cobb.*/
            {
                output("[%s] following Cobb. to Level [%d] to meet my father\n", 
                       dattr->name, dattr->level+1);
                dream_level_create(dattr->level+1, dream_level_2, dattr);
            }
            else if(req->cmd == DREAMER_FAKE_SHAPE)
            {
                output("[%s] interacting with Mr. Browning in hijacked state at level [%d]\n",
                       dattr->name, dattr->level);
            }
            else if(req->cmd == DREAMER_KICK_BACK)
            {
                free(req);
                output("[%s] got a Kick at level [%d]. Exiting back to reality with the inception thought\n",
                       dattr->name, dattr->level);
                goto out;
            }
            free(req);
        }
        assert(clock_gettime(CLOCK_REALTIME, &ts)==0);
        ts.tv_sec += 2;
        pthread_cond_timedwait(dattr->cond[0], &dreamer_mutex[0], &ts);
    }
    out:
    pthread_mutex_unlock(&dreamer_mutex[0]);
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

    /*
     * All others wait for Fischers projections to throw up. at their defense.
     */
    while(! (req = dream_dequeue_cmd(dattr) ) )
    {
        pthread_mutex_unlock(&dreamer_mutex[0]);
        usleep(10000); 
        pthread_mutex_lock(&dreamer_mutex[0]);
    }
    assert(req->cmd == DREAMER_DEFENSE_PROJECTIONS);
    free(req);
    output("[%s] sees Fischers defense projections at work in his dream\n", dattr->name);
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
        }
        break;
        
    case DREAM_WORLD_ARCHITECT:
        {
            output("[%s] following Cobb to level [%d]\n", dattr->name, dattr->level+1);
            dream_level_create(dattr->level+1, dream_level_2, dattr);
        }
        break;

    case DREAM_ORGANIZER:
        {
            struct dreamer_attr *self = NULL;
            struct dreamer_request *req = NULL;
            output("[%s] follows Cobb. to level 2 to fight Fischers projections\n",
                   dattr->name);
            dream_level_create(dattr->level+1, dream_level_2, dattr);
            for(;;)
            {
                struct timespec ts = {0};
                while ( (req = dream_dequeue_cmd(dattr) ) )
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
                            dream_enqueue_cmd(self, DREAMER_FREE_FALL, dattr, self->level);
                        }
                    }
                    else if(req->cmd == DREAMER_KICK_BACK)
                    {
                        output("[%s] got a Kick at level [%d]. Exiting back to level [%d]\n",
                               dattr->name, dattr->level, dattr->level-1);
                        goto out_unlock;
                    }
                    free(req);
                }
                if(self) /* send FIGHT instructions to upper level self */
                {
                    dream_enqueue_cmd(self, DREAMER_FIGHT, dattr, self->level);
                }
                assert(clock_gettime(CLOCK_REALTIME, &ts)==0);
                ts.tv_sec += 2;
                pthread_cond_timedwait(dattr->cond[0], &dreamer_mutex[0], &ts);
            }
            
        }
        break;

    case DREAM_SHAPES_FAKER: /* Eames*/
        {
            /*
             * Fake Fischers right hand: Mr Browning for Fischer to confuse Fischer
             */
            output("[%s] faking Browning to manipulate Fischers emotions for the inception at level 1\n",
                   dattr->name);
            dream_enqueue_cmd(fischer_level1, DREAMER_FAKE_SHAPE, dattr, 1);
            output("[%s] follows Cobb to level 2 to continue with the manipulation of Fischer\n",
                   dattr->name);
            dream_level_create(dattr->level+1, dream_level_2, dattr);
            for(;;)
            {
                struct timespec ts = {0};
                while( (req = dream_dequeue_cmd(dattr)) )
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
                dream_enqueue_cmd(fischer_level1, DREAMER_FAKE_SHAPE, dattr, 1);
                assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
                ts.tv_sec += 2;
                pthread_cond_timedwait(dattr->cond[0], &dreamer_mutex[0], &ts);
            }
        }
        break;

    case DREAM_SEDATIVE_CREATOR:  /* Yusuf stays in level 1 */
        {
            continue_dreaming_in_level_1(dattr);
        }
        break;

    case DREAM_OVERLOOKER: /* Saito: got hit but follows into level 2 */
        {
            output("[%s] shot in level 1. Following Cobb to level 2\n", dattr->name);
            dattr->shared_state |= DREAMER_SHOT;
            output("[%s] follows Cobb. to level 2 after being shot\n",dattr->name);
            dream_level_create(dattr->level+1, dream_level_2, dattr);
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
    pthread_mutex_unlock(&dreamer_mutex[0]);
}

static void shared_dream_level_1(void *dreamer_attr)
{
    struct dreamer_attr *dattr = dreamer_attr;

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
            fischer_level1_taskid = syscall(SYS_gettid);
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
            memset(fischers_mind_state, 0x90, getpagesize()); /*fill with x86 NOP opcodes*/
            memcpy(fischers_mind_state, fischers_thoughts, sizeof(fischers_thoughts));
            asm("push fischers_mind_state\n"\
                "jmp fischer_dream_level1\n");
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
    pthread_cond_broadcast(&inception_reality_wakeup_for_all);
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
#if 0
    asm(".section .text\n"
        ".align 8\n"
        ".byte 0xe9\n" /* fool linker to enable relative addressing */
        ".long 0x1e\n"   /* relative JMP call to 0x1e or call instruction below */
#ifdef __i386__
        "popl %ecx\n"
#else
        "popq %rcx\n"
#endif
        "mov $"STR(__NR_write)",%eax\n" 
        "movl $1, %ebx\n"\
        "movl $55, %edx\n"
        "int $0x80\n"
        "movl $"STR(__NR_exit)",%eax\n"
        "movl $0, %ebx\n"
        "int $0x80\n"
        ".byte 0xe8\n"
        ".long -0x23\n"/*"call -0x23\n"*/
        ".string \"Reconcile with my father and have my own individuality\\n\"");
        
#endif
#if 1
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
#endif
    return 0;
}
    



