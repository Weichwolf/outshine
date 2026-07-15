/* Unit tests — tiles/lru.h
 *
 * The DEM cache's eviction policy. It used to live inside elev.c between a network fetch and a
 * PNG decode, so exercising it meant standing up a real tile server.
 *
 * Worth pinning because an LRU never fails loudly: evict the wrong slot and every answer is still
 * correct, you just re-fetch tiles you already had. The only symptom is a service that is
 * inexplicably slow — and "slower than it should be" is not something a test suite notices, nor
 * a human, for a long time. These assert the policy directly.
 */
#include "tassert.h"
#include "../../tiles/lru.h"

#define N 4

void test_lru(void){
    tsection("lru: find + claim");
    {
        fb_lru_slot slots[N]; fb_lru l; fb_lru_init(&l, slots, N);
        ck(fb_lru_find(&l, 13, 100, 200) == -1, "an empty table is all misses");
        for(int i = 0; i < N; i++) ck(!fb_lru_occupied(&l, i), "init leaves every slot free");

        int s = fb_lru_victim(&l);
        ck(s == 0, "the first victim is the first free slot");
        fb_lru_claim(&l, s, 13, 100, 200);
        ck(fb_lru_occupied(&l, 0), "a claimed slot is occupied");
        ck(fb_lru_find(&l, 13, 100, 200) == 0, "a claimed entry is then found");
        ck(fb_lru_find(&l, 13, 100, 201) == -1, "a different y is a different tile");
        ck(fb_lru_find(&l, 13, 101, 200) == -1, "a different x is a different tile");
        ck(fb_lru_find(&l, 14, 100, 200) == -1, "a different z is a different tile");

        /* free slots must be used before anything is evicted */
        ck(fb_lru_victim(&l) == 1, "a free slot is preferred over evicting a live one");
        fb_lru_claim(&l, 1, 13, 101, 200);
        ck(fb_lru_find(&l, 13, 100, 200) == 0 && fb_lru_find(&l, 13, 101, 200) == 1,
           "two entries coexist in their own slots");
    }

    tsection("lru: eviction picks the least-recently-used");
    {
        fb_lru_slot slots[N]; fb_lru l; fb_lru_init(&l, slots, N);
        for(int i = 0; i < N; i++) fb_lru_claim(&l, fb_lru_victim(&l), 13, i, 0);
        /* table is full; slot 0 is the oldest */
        ck(fb_lru_victim(&l) == 0, "a full table evicts the oldest entry");

        /* touching the oldest must save it — that is the whole point of an LRU */
        ck(fb_lru_find(&l, 13, 0, 0) == 0, "the oldest entry is still resident");
        ck(fb_lru_victim(&l) == 1, "a hit on the oldest makes it recent; the NEXT oldest goes");

        /* re-reading in order rotates the victim exactly the way you would expect */
        fb_lru_find(&l, 13, 1, 0);
        ck(fb_lru_victim(&l) == 2, "victim follows use order (2 is now oldest)");
        fb_lru_find(&l, 13, 2, 0);
        ck(fb_lru_victim(&l) == 3, "victim follows use order (3 is now oldest)");

        /* the classic bug this pins: evicting on WRITE order rather than USE order.
         * Slot 0 was written first but read most recently, so it must NOT be the victim. */
        ck(fb_lru_victim(&l) != 0, "eviction follows USE order, not insertion order");
    }

    tsection("lru: reuse of an evicted slot");
    {
        fb_lru_slot slots[N]; fb_lru l; fb_lru_init(&l, slots, N);
        for(int i = 0; i < N; i++) fb_lru_claim(&l, fb_lru_victim(&l), 13, i, 0);

        int v = fb_lru_victim(&l);
        ck(v == 0, "oldest is slot 0");
        ck(fb_lru_occupied(&l, v), "the victim is occupied -> the caller must free its payload");
        fb_lru_claim(&l, v, 13, 99, 99);                 /* caller freed the grid, now reuses it */
        ck(fb_lru_find(&l, 13, 0, 0) == -1, "the evicted tile is gone");
        ck(fb_lru_find(&l, 13, 99, 99) == 0, "the new tile lives in the reused slot");
        ck(fb_lru_find(&l, 13, 1, 0) == 1, "the other entries survived the eviction");

        /* the table must never grow past its slots, however many tiles we push through it */
        int inrange = 1;
        for(int i = 0; i < 500; i++){
            int s = fb_lru_victim(&l);
            if(s < 0 || s >= N) inrange = 0;
            fb_lru_claim(&l, s, 13, 1000+i, 7);
        }
        ck(inrange, "victim is always a valid slot index, over 500 inserts");
        int live = 0; for(int i = 0; i < N; i++) if(fb_lru_occupied(&l, i)) live++;
        ck(live == N, "after 500 inserts the table still holds exactly N entries");
        ck(fb_lru_find(&l, 13, 1499, 7) >= 0, "the most recent insert is resident");
        ck(fb_lru_find(&l, 13, 1000, 7) == -1, "a long-evicted insert is gone");

        /* no duplicates: the same tile must occupy exactly one slot */
        fb_lru_slot s2[N]; fb_lru l2; fb_lru_init(&l2, s2, N);
        fb_lru_claim(&l2, fb_lru_victim(&l2), 13, 5, 5);
        int first = fb_lru_find(&l2, 13, 5, 5);
        int count = 0; for(int i = 0; i < N; i++) if(s2[i].valid && s2[i].x == 5 && s2[i].y == 5) count++;
        ck(count == 1 && first == 0, "a tile occupies exactly one slot");
    }

    tsection("lru: a single-slot table still behaves");
    {
        fb_lru_slot one[1]; fb_lru l; fb_lru_init(&l, one, 1);
        ck(fb_lru_victim(&l) == 0, "the only slot is the victim");
        fb_lru_claim(&l, 0, 13, 1, 1);
        ck(fb_lru_find(&l, 13, 1, 1) == 0, "it holds its one tile");
        ck(fb_lru_victim(&l) == 0, "a full 1-slot table evicts its only entry");
        fb_lru_claim(&l, 0, 13, 2, 2);
        ck(fb_lru_find(&l, 13, 1, 1) == -1 && fb_lru_find(&l, 13, 2, 2) == 0,
           "the single slot is replaced, not corrupted");
    }
}
