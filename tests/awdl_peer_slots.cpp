// Host checks for the AWDL peer slots (AWDLPeerSlot.hpp): how many slots a listener uses, where an ADDBA request
// waits, and which slot a peer's frames belong to. No firmware or radio.
#include <AWDLPeerSlot.hpp>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
using namespace ItlAWDL;

static AddbaRequest request(uint8_t last, uint8_t tid = 0) {
    AddbaRequest r{};
    r.immediate = true;
    r.peer[0] = 2;
    r.peer[5] = last;
    r.tid = tid;
    return r;
}
static void add(PeerSlot &s, uint8_t last) {
    s.added = true;
    memset(s.addr, 0, 6);
    s.addr[0] = 2;
    s.addr[5] = last;
}

int main() {
    unsigned cases = 0;
    // Stations: slot 0 is 3, then 5 and up; 4 is the scheduler's channel-6 station and never a slot's.
    assert(stationForSlot(0) == 3 && stationForSlot(1) == 5 && stationForSlot(11) == 15);
    for (unsigned i = 0; i < kMaxPeerSlots; ++i)
        assert(stationForSlot(i) != kChannel6Station && stationForSlot(i) < 16);
    cases += 2;
    // Slot count: one without -itlawdlmultipeer; else one per usable station below the firmware's count.
    for (unsigned n : {0U, 3U, 4U, 8U, 16U})
        assert(usablePeerSlots(false, n) == 1), ++cases;
    assert(usablePeerSlots(true, 0) == kMaxPeerSlots && usablePeerSlots(true, 16) == kMaxPeerSlots);
    assert(usablePeerSlots(true, 15) == 11);                               // the AX211: stations 3 and 5-14
    assert(usablePeerSlots(true, 8) == 4 && usablePeerSlots(true, 6) == 2 && usablePeerSlots(true, 5) == 1);
    assert(usablePeerSlots(true, 4) == 1 && usablePeerSlots(true, 3) == 1 && usablePeerSlots(true, 1) == 1);
    cases += 7;

    // One slot: exactly the rule from before slots (slot 0 unless its station is another peer's).
    for (bool added : {false, true})
        for (bool same : {false, true})
            for (bool waiting : {false, true}) {
                PeerSlot slots[kMaxPeerSlots];
                if (added)
                    add(slots[0], 3);
                if (waiting)
                    assert(slots[0].requests.note(request(added ? 3 : 7)));
                const AddbaRequest r = request(same ? 3 : 4);
                PeerSlot *got = slotForRequest(slots, 1, r.peer, 5);
                assert(got == (added && !same ? nullptr : &slots[0]));
                ++cases;
            }
    assert(!slotForRequest(nullptr, 0, request(3).peer, 1));

    // Several slots: a peer keeps its slot, a new peer takes the first free one, a full pool says none.
    {
        PeerSlot slots[kMaxPeerSlots];
        for (unsigned i = 0; i < kMaxPeerSlots; ++i)
            slots[i].station = stationForSlot(i);
        const AddbaRequest a = request(10), b = request(11), c = request(12);
        PeerSlot *sa = slotForRequest(slots, 3, a.peer, 1);
        assert(sa == &slots[0] && sa->requests.note(a));
        assert(slotForRequest(slots, 3, a.peer, 1) == &slots[0]);              // waiting requests keep the slot
        PeerSlot *sb = slotForRequest(slots, 3, b.peer, 1);
        assert(sb == &slots[1] && sb->requests.note(b));
        add(slots[0], 10);
        slots[0].requests.clear();
        assert(slotForRequest(slots, 3, a.peer, 1) == &slots[0]);              // an added station keeps it too
        PeerSlot *sc = slotForRequest(slots, 3, c.peer, 1);
        assert(sc == &slots[2] && sc->requests.note(c));
        assert(!slotForRequest(slots, 3, request(13).peer, 1));                 // full
        assert(slotForRequest(slots, 4, request(13).peer, 1) == &slots[3]);
        // A slot that failed in this generation isn't handed out; in the next one it is again.
        slots[3].failedGen = 1;
        assert(!slotForRequest(slots, 4, request(13).peer, 1));
        assert(slotForRequest(slots, 4, request(13).peer, 2) == &slots[3]);
        // Slot 0 is never skipped for its failure mark (its failures quarantine all Block Ack instead).
        PeerSlot fresh[kMaxPeerSlots];
        fresh[0].failedGen = 1;
        assert(slotForRequest(fresh, 2, a.peer, 1) == &fresh[0]);
        // Frames belong to the slot whose station is the sender's, and only an added one.
        assert(addedSlotFor(slots, 3, a.peer) == &slots[0]);
        assert(!addedSlotFor(slots, 3, b.peer));                               // still only asking
        add(slots[1], 11);
        assert(addedSlotFor(slots, 3, b.peer) == &slots[1]);
        assert(!addedSlotFor(slots, 1, b.peer));                               // beyond the usable slots
        assert(!addedSlotFor(slots, 3, request(99).peer));
        cases += 16;
    }
    // The pending requests' peer.
    {
        PendingAddbaRequests q;
        assert(!q.peer());
        assert(q.note(request(5, 3)) && samePeer(q.peer(), request(5).peer));
        assert(!q.note(request(6)));                                           // one peer per queue
        q.discard(3);
        assert(!q.peer() && q.note(request(6)) && samePeer(q.peer(), request(6).peer));
        assert(!samePeer(nullptr, request(6).peer) && !samePeer(request(6).peer, nullptr));
        cases += 5;
    }
    // Peer stations: 3 and 5-15; never 0-2 or channel 6's 4.
    for (unsigned st = 0; st < 20; ++st)
        assert(isPeerStation(st) == (st == 3 || (st >= 5 && st <= 15)));
    ++cases;
    // The hold for a further slot's frames: first in, first out, bounded, nothing lost or reordered.
    {
        BoundedFifo<int, 4> q;
        int v = 0;
        assert(q.empty() && !q.pop(v) && q.size() == 0);
        for (int i = 1; i <= 4; ++i)
            assert(q.push(i));
        assert(q.full() && !q.push(5) && q.front() == 1);
        assert(q.pop(v) && v == 1 && q.push(5));                          // wraps around
        for (int want = 2; want <= 5; ++want)
            assert(q.pop(v) && v == want);
        assert(q.empty() && !q.pop(v) && v == 5);
        for (int round = 0; round < 50; ++round) {                         // interleaved, across many wraps
            assert(q.push(round) && q.push(round + 1000));
            assert(q.pop(v) && v == round && q.pop(v) && v == round + 1000);
        }
        cases += 4;
    }
    printf("AWDL peer slots: %u cases passed (host only)\n", cases);
}
