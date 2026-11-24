// our frames advertise the schedule the radio really follows (include/AWDLPeerProbe.hpp TLV 18,
// include/AWDLLocalSchedule.hpp's sync TLV sequence), and nothing changes when no schedule is given.
#include <AWDLLocalSchedule.hpp>
#include <AWDLPeerProbe.hpp>
#include <assert.h>
#include <stdio.h>
#include <string.h>

using namespace ItlAWDL;

int main() {
    // The executed plan for this Mac: macOS's 149 runs, the station (36) elsewhere, slot 8 (6) left to the station.
    const uint8_t exec[16] = {149, 149, 149, 149, 149, 149, 36, 36, 36, 149, 149, 149, 149, 149, 36, 36};
    const uint8_t own[6] = {2, 1, 2, 3, 4, 5};

    // Sync TLV: the legacy sequence carries the plan, each slot with its band's flags; the prefix is unchanged.
    {
        SyncPrefix five = {149, 149, 0, 0, 16, 110, 0x1000, 16, 16, 0, 3, 3, 3, 3, {2, 1, 2, 3, 4, 5}};
        uint8_t fixed[73], planned[73];
        size_t n = 0, m = 0;
        assert(buildLocalFixedSchedule(five, 0, 123456, 0, fixed, 73, &n) == BuildResult::Ready && n == 73);
        assert(buildLocalFixedSchedule(five, 0, 123456, 0, planned, 73, &m, exec) == BuildResult::Ready && m == 73);
        assert(!memcmp(fixed, planned, 39));   // everything before the sequence entries is the same
        for (unsigned i = 0; i < 16; ++i) {
            assert(planned[40 + 2 * i] == exec[i]);
            assert(planned[39 + 2 * i] == 0x1b);   // all 5 GHz here
        }
        assert(!memcmp(fixed + 71, planned + 71, 2));
        // A null schedule is exactly the old frame.
        uint8_t again[73];
        size_t k = 0;
        assert(buildLocalFixedSchedule(five, 0, 123456, 0, again, 73, &k, nullptr) == BuildResult::Ready);
        assert(!memcmp(again, fixed, 73));
        // A slot that can't be described (radar channel 52, or 0) refuses the frame; the caller falls back.
        uint8_t bad[16];
        memcpy(bad, exec, 16);
        bad[7] = 52;
        assert(buildLocalFixedSchedule(five, 0, 123456, 0, planned, 73, &m, bad) == BuildResult::Invalid);
        bad[7] = 0;
        assert(buildLocalFixedSchedule(five, 0, 123456, 0, planned, 73, &m, bad) == BuildResult::Invalid);
        // A 2.4 GHz slot gets the 2.4 GHz flags.
        uint8_t mixed[16];
        memcpy(mixed, exec, 16);
        mixed[8] = 6;
        assert(buildLocalFixedSchedule(five, 0, 123456, 0, planned, 73, &m, mixed) == BuildResult::Ready);
        assert(planned[39 + 16] == 0x2b && planned[40 + 16] == 6);
    }

    // TLV 18: per-slot channels; the listener's channel keeps the given class (80 MHz here), others get their
    // 20 MHz class. A template with an election v2 TLV, as macOS sends it.
    {
        uint8_t base[400] = {};
        size_t baseLen = 0;
        const uint8_t first[4] = {6, 1, 0, 0x11};
        memcpy(base, first, 4);
        baseLen = 4;
        base[baseLen] = 24; base[baseLen + 1] = 40; base[baseLen + 2] = 0;
        base[baseLen + 3 + 24] = 65;
        baseLen += 43;

        uint8_t fixed[400], planned[400];
        memcpy(fixed, base, 400);
        memcpy(planned, base, 400);
        size_t fixedLen = baseLen, plannedLen = baseLen;
        assert(extendWithPeerTLVs(fixed, &fixedLen, sizeof(fixed), own, 149, 128));
        assert(extendWithPeerTLVs(planned, &plannedLen, sizeof(planned), own, 149, 128, false, exec));
        assert(fixedLen == plannedLen);
        const uint8_t *cs = planned + 24;
        assert(cs[0] == 18 && cs[1] == 41 && cs[3] == 15 && cs[4] == 3 && cs[6] == 3 && cs[7] == 0xff);
        for (unsigned i = 0; i < 16; ++i) {
            assert(cs[9 + 2 * i] == exec[i]);
            assert(cs[10 + 2 * i] == (exec[i] == 149 ? 128 : 115));
        }
        // Only the 16 entries differ from the fixed frame.
        assert(!memcmp(fixed, planned, 24 + 9));
        assert(!memcmp(fixed + 24 + 41 + 3, planned + 24 + 41 + 3, plannedLen - 24 - 44));
        // No schedule: the old frame, byte for byte.
        uint8_t again[400];
        memcpy(again, base, 400);
        size_t againLen = baseLen;
        assert(extendWithPeerTLVs(again, &againLen, sizeof(again), own, 149, 128, false, nullptr));
        assert(againLen == fixedLen && !memcmp(again, fixed, fixedLen));
        // A bad slot refuses without writing.
        uint8_t bad[16];
        memcpy(bad, exec, 16);
        bad[3] = 52;
        uint8_t refused[400];
        memcpy(refused, base, 400);
        size_t refusedLen = baseLen;
        assert(!extendWithPeerTLVs(refused, &refusedLen, sizeof(refused), own, 149, 128, false, bad));
        assert(refusedLen == baseLen && !memcmp(refused, base, 400));
    }

    // Master Indication TLVs pass the schedule through to TLV 18.
    {
        uint8_t mi[400] = {};
        size_t miLen = 0;
        mi[0] = 24; mi[1] = 40; mi[2] = 0; mi[3 + 24] = 65; miLen = 43;
        uint8_t out[400];
        size_t written = 0;
        assert(buildMasterIndicationTLVs(mi, miLen, own, out, sizeof(out), &written, 149, 128, false, exec));
        for (unsigned i = 0; i < 16; ++i) assert(out[24 + 9 + 2 * i] == exec[i]);
    }

    printf("executed plan: sync TLV sequence, TLV 18 and MI pass-through; null keeps the old frames: passed\n");
    return 0;
}
