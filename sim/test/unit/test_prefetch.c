/* Unit tests — tiles/prefetch.h
 *
 * The queue that keeps the OSM render and the aerial photo warm for the same ground, so the
 * renderer's TAB switch is a blit and not a download. It matters because that switch is meant as
 * a fallback for when the camera cannot deliver, and a fallback that waits on the network is not
 * one.
 *
 * The property worth pinning is the unglamorous one: this queue must NEVER block and NEVER grow.
 * It sits in a single-threaded accept() loop serving a live flight view. If prefetching cannot
 * keep up, forgetting work is correct; stalling the server or eating memory is not. A ring buffer
 * that silently wraps past its own head instead of dropping would corrupt exactly under the load
 * it was built for — and would look fine in every gentle test.
 */
#include "tassert.h"
#include "../../tiles/prefetch.h"

void test_prefetch(void){
    tsection("prefetch: push / pop / order");
    {
        fb_pf_queue p; fb_pf_init(&p);
        ck(fb_pf_count(&p) == 0, "a fresh queue is empty");
        fb_pf_job j;
        ck(!fb_pf_pop(&p, &j), "popping an empty queue reports empty");
        ck(p.pushed == 0 && p.dropped == 0 && p.deduped == 0, "fresh counters");

        ck(fb_pf_push(&p, 2, 14, 100, 200, 1024) == 1, "a push is accepted");
        ck(fb_pf_count(&p) == 1, "count follows the push");
        ck(fb_pf_has(&p, 2, 14, 100, 200, 1024), "the job is queued");
        ck(!fb_pf_has(&p, 0, 14, 100, 200, 1024), "a different kind is a different job");
        ck(!fb_pf_has(&p, 2, 15, 100, 200, 1024), "a different zoom is a different job");
        ck(!fb_pf_has(&p, 2, 14, 101, 200, 1024), "a different x is a different job");
        ck(!fb_pf_has(&p, 2, 14, 100, 201, 1024), "a different y is a different job");

        ck(fb_pf_pop(&p, &j) == 1, "pop returns the job");
        ck(j.kind == 2 && j.z == 14 && j.x == 100 && j.y == 200, "pop returns it intact");
        ck(fb_pf_count(&p) == 0, "the queue is empty again");

        /* FIFO: tiles are queued in the order the renderer asked for their neighbours, and the
         * ring order upstream means the nearest ground is queued first. Popping out of order
         * would fetch the horizon before the ground under the aircraft. */
        fb_pf_init(&p);
        for(int i = 0; i < 5; i++) fb_pf_push(&p, 2, 14, i, 0, 1024);
        int ok = 1;
        for(int i = 0; i < 5; i++){ fb_pf_pop(&p, &j); if(j.x != i) ok = 0; }
        ck(ok, "the queue is FIFO (nearest-first order survives)");
    }

    tsection("prefetch: de-duplication");
    {
        fb_pf_queue p; fb_pf_init(&p);
        ck(fb_pf_push(&p, 2, 14, 1, 1, 1024) == 1, "first push of a tile is queued");
        ck(fb_pf_push(&p, 2, 14, 1, 1, 1024) == 0, "the same tile again is refused");
        ck(fb_pf_count(&p) == 1, "...and does not occupy a second slot");
        ck(p.deduped == 1, "the dedup is counted, not silent");
        ck(fb_pf_push(&p, 2, 14, 1, 2, 1024) == 1, "a genuinely different tile is still queued");
        /* the texture size is part of the identity: the near tier wants 1024, the far tier 512,
         * and they are different images of the same ground */
        ck(fb_pf_push(&p, 2, 14, 1, 1, 512) == 1, "the same tile at another texture size is other work");
        ck(fb_pf_push(&p, 2, 14, 1, 1, 512) == 0, "...and dedups on its own");
        fb_pf_job jj; fb_pf_pop(&p, &jj); fb_pf_pop(&p, &jj); fb_pf_pop(&p, &jj);


        /* an area being hammered must not fill the queue with one tile */
        fb_pf_init(&p);
        fb_pf_push(&p, 2, 14, 1, 1, 1024);
        for(int i = 0; i < 100; i++) fb_pf_push(&p, 2, 14, 1, 1, 1024);
        ck(fb_pf_count(&p) == 1, "100 repeats of a queued tile add nothing");

        /* but once popped, it may legitimately be queued again */
        fb_pf_job j; fb_pf_pop(&p, &j);
        ck(fb_pf_push(&p, 2, 14, 1, 1, 1024) == 1, "after being popped, a tile can be queued again");
    }

    tsection("prefetch: full queue drops, never blocks or wraps");
    {
        fb_pf_queue p; fb_pf_init(&p);
        int accepted = 0;
        for(int i = 0; i < FB_PF_CAP + 500; i++)
            if(fb_pf_push(&p, 1, 14, i, 0, 1024)) accepted++;

        ck(accepted == FB_PF_CAP - 1, "a full queue accepts exactly its capacity, then stops");
        ck(fb_pf_count(&p) == FB_PF_CAP - 1, "count never exceeds capacity");
        ck(p.dropped == 501, "every refused push is counted (silence would hide the overload)");

        /* THE bug this exists to prevent: a ring that wraps past its own head. The first job
         * pushed must still be the first job popped -- not overwritten by a later one. */
        fb_pf_job j;
        ck(fb_pf_pop(&p, &j) == 1 && j.x == 0, "the oldest job survived the overflow (no wrap)");
        ck(fb_pf_pop(&p, &j) == 1 && j.x == 1, "and the next one after it");

        /* draining a full queue must yield exactly what went in, and then stop */
        fb_pf_init(&p);
        for(int i = 0; i < FB_PF_CAP - 1; i++) fb_pf_push(&p, 1, 14, i, 0, 1024);
        int drained = 0; while(fb_pf_pop(&p, &j)) drained++;
        ck(drained == FB_PF_CAP - 1, "a full queue drains to exactly what it accepted");
        ck(fb_pf_count(&p) == 0, "and is then empty");
        ck(!fb_pf_pop(&p, &j), "a drained queue reports empty rather than replaying");
    }

    tsection("prefetch: the ring keeps working across wrap-around");
    {
        /* The server runs for hours: head and tail cross the end of the array again and again.
         * Push/pop in a loop far past capacity and the queue must behave identically throughout. */
        fb_pf_queue p; fb_pf_init(&p);
        fb_pf_job j; int ok = 1;
        for(int i = 0; i < FB_PF_CAP * 3; i++){
            if(!fb_pf_push(&p, 2, 16, i, 7, 1024)) ok = 0;
            if(!fb_pf_pop(&p, &j) || j.x != i) ok = 0;
        }
        ck(ok, "push/pop stays correct over 3x capacity of wrap-around");
        ck(p.dropped == 0, "a queue that is drained as fast as it fills drops nothing");
        ck(fb_pf_count(&p) == 0, "and ends empty");
    }
}
