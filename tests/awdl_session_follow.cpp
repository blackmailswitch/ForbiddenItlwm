// when the follower asks the firmware for listener airtime (include/AWDLSessionFollow.hpp).
#include <AWDLSessionFollow.hpp>
#include <assert.h>
#include <stdio.h>

using namespace ItlAWDL;

static const uint64_t kEaw = 64 * 1024;   // one entry, µs

// A peer schedule anchored so that entry 0 (eaw 0) starts at `t0`, with the given 16 channels.
static PeerSchedule anchored(uint64_t t0, const uint8_t (&seq)[16]) {
    PeerSchedule s{};
    s.assumeZero = true;
    PeerSyncInfo info{};
    info.awCounter = 0;
    info.downCounterTU = 64;
    info.awPeriodTU = 16;
    memcpy(info.channels, seq, sizeof(seq));
    s.note(info, t0, 149);
    return s;
}

int main() {
    const uint64_t t0 = 10000000;
    // The iPhone's 149 entries: 0-2 and 9-10, others away.
    const uint8_t seq[16] = {149, 149, 149, 0, 0, 0, 0, 0, 6, 149, 149, 0, 0, 0, 0, 0};
    PeerSchedule s = anchored(t0, seq);
    assert(s.fresh(t0) && s.known());
    FollowRun r{};

    // In entry 0: the run in progress, 3 entries, ending at entry 3's start.
    assert(nextPeerRun(s, t0, &r) && r.inProgress && r.entries == 3 && r.endUS == t0 + 3 * kEaw);
    FollowDecision d = followDecide(&s, t0 + 1000, 0);
    assert(d.action == FollowAction::SendNow && d.endUS == t0 + 3 * kEaw && d.entries == 3);

    // Near the end of entry 2 (less than one AW of the run left): that run is skipped, the next is 9-10.
    const uint64_t late2 = t0 + 3 * kEaw - 10000;
    assert(nextPeerRun(s, late2, &r) && !r.inProgress && r.startUS == t0 + 9 * kEaw && r.entries == 2);

    // At entry 3: the next run (entry 9) starts in 6 entries, 393 ms, beyond the arm window: nothing yet.
    d = followDecide(&s, t0 + 3 * kEaw, 0);
    assert(d.action == FollowAction::None);

    // At entry 6: entry 9 starts in 196.6 ms: arm, to send 7 ms before it.
    d = followDecide(&s, t0 + 6 * kEaw, 0);
    assert(d.action == FollowAction::Arm && d.sendAtUS == t0 + 9 * kEaw - kFollowLeadUS && d.endUS == t0 + 11 * kEaw);

    // Within the lead of the run's start: send now.
    d = followDecide(&s, t0 + 9 * kEaw - 3000, 0);
    assert(d.action == FollowAction::SendNow && d.endUS == t0 + 11 * kEaw);

    // The next sequence repeats: entry 16 (= entry 0) after entry 11.
    assert(nextPeerRun(s, t0 + 11 * kEaw, &r) && r.startUS == t0 + 16 * kEaw && r.entries == 3);

    // No schedule: a duty session, then none until a sequence has passed.
    d = followDecide(nullptr, t0, 0);
    assert(d.action == FollowAction::Duty);
    d = followDecide(nullptr, t0 + 500000, t0);
    assert(d.action == FollowAction::None);
    d = followDecide(nullptr, t0 + kFollowDutyPeriodUS, t0);
    assert(d.action == FollowAction::Duty);

    // A stale schedule (no sync frame for over 2 s) counts as none.
    const uint64_t stale = t0 + kPeerScheduleStaleUS + 1;
    d = followDecide(&s, stale, stale - 1000);
    assert(d.action == FollowAction::None);   // a duty session went 1 ms ago
    d = followDecide(&s, stale, stale - kFollowDutyPeriodUS);
    assert(d.action == FollowAction::Duty);

    // A peer on our channel in every entry: runs are capped at 6 entries.
    uint8_t all[16];
    memset(all, 149, sizeof(all));
    PeerSchedule full = anchored(t0, all);
    assert(nextPeerRun(full, t0, &r) && r.entries == kFollowMaxEntries);

    // A peer never on our channel: no run, duty instead.
    const uint8_t none[16] = {44, 44, 44, 44, 0, 0, 0, 0, 6, 44, 44, 44, 0, 0, 0, 0};
    PeerSchedule away = anchored(t0, none);
    assert(!nextPeerRun(away, t0, &r));
    assert(followDecide(&away, t0, 0).action == FollowAction::Duty);

    // Session lengths: clamped to 6 entries, refused below one AW.
    assert(followDurationTU(t0, t0 + 3 * kEaw) == 192);
    assert(followDurationTU(t0, t0 + 20 * kEaw) == kFollowMaxEntries * 64);
    assert(followDurationTU(t0, t0 + 10000) == 0);
    assert(followDurationTU(t0 + 5, t0) == 0);

    // macOS's plan on the Mac's own clock (entry n at [n * 64 TU, (n + 1) * 64 TU) of uptime).
    // This Mac's plan (*Live*): 149 x6, 36 x2, 6, 149 x5, 36 x2.
    const uint8_t plan[16] = {149, 149, 149, 149, 149, 149, 36, 36, 6, 149, 149, 149, 149, 149, 36, 36};
    const uint64_t seq0 = 1000 * 16 * kPlanEawUS;   // the start of a sequence (entry 0)
    assert(nextPlanRun(plan, 149, seq0, &r) && r.inProgress && r.startUS == seq0 && r.entries == 6 &&
           r.endUS == seq0 + 6 * kPlanEawUS);
    // In entry 6 (station): next is entry 9, five entries.
    assert(nextPlanRun(plan, 149, seq0 + 6 * kPlanEawUS + 100, &r) && !r.inProgress &&
           r.startUS == seq0 + 9 * kPlanEawUS && r.entries == 5);
    // At entry 6's start, entry 9 starts 3 entries (196.6 ms) later: arm, 7 ms early.
    d = followDecidePlan(&plan, 149, seq0 + 6 * kPlanEawUS, 0);
    assert(d.action == FollowAction::Arm && d.sendAtUS == seq0 + 9 * kPlanEawUS - kFollowLeadUS &&
           d.endUS == seq0 + 14 * kPlanEawUS && d.entries == 5);
    // After entry 13: the next run is the next sequence's entries 0-5.
    assert(nextPlanRun(plan, 149, seq0 + 14 * kPlanEawUS, &r) && r.startUS == seq0 + 16 * kPlanEawUS && r.entries == 6);
    // Mid-run with too little left (entry 5's last 5 ms): skip to entry 9.
    assert(nextPlanRun(plan, 149, seq0 + 6 * kPlanEawUS - 5000, &r) && r.startUS == seq0 + 9 * kPlanEawUS);
    // Channel 6's single entry is a run of one on 6.
    assert(nextPlanRun(plan, 6, seq0, &r) && r.startUS == seq0 + 8 * kPlanEawUS && r.entries == 1);
    // No plan: duty; a plan without our channel: nothing (no duty, the plan says we're not needed there).
    assert(followDecidePlan(nullptr, 149, seq0, 0).action == FollowAction::Duty);
    const uint8_t elsewhere[16] = {44, 44, 44, 44, 44, 44, 36, 36, 6, 44, 44, 44, 44, 44, 36, 36};
    assert(followDecidePlan(&elsewhere, 149, seq0, 0).action == FollowAction::None);
    assert(!nextPlanRun(plan, 0, seq0, &r));
    // Counts: 11 on the listener's channel, 4 on the station's, 1 other (slot 8, channel 6).
    unsigned listener = 0, station = 0, other = 0;
    planCounts(plan, 149, 36, &listener, &station, &other);
    assert(listener == 11 && station == 4 && other == 1);

    printf("follower: runs, arming, lead, duty, staleness, caps: passed\n");
    printf("plan follower: macOS's plan on the Mac's clock, runs, arming, wrap, counts: passed\n");
    // a longer lead requests the session earlier and keeps the end; the run start is reported.
    {
        uint8_t plan[16] = {};
        plan[9] = 149;
        const uint64_t seq = 100 * 16 * kPlanEawUS;
        const FollowDecision a = followDecidePlan(&plan, 149, seq + 8 * kPlanEawUS, 0);
        const FollowDecision b = followDecidePlan(&plan, 149, seq + 8 * kPlanEawUS, 0, ownClock(), 12000);
        assert(a.action == FollowAction::Arm && b.action == FollowAction::Arm);
        assert(a.sendAtUS == seq + 9 * kPlanEawUS - kFollowLeadUS && b.sendAtUS == seq + 9 * kPlanEawUS - 12000);
        assert(a.endUS == b.endUS && a.runStartUS == seq + 9 * kPlanEawUS && b.runStartUS == a.runStartUS);
    }
    return 0;
}
