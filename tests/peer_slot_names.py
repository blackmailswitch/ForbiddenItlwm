"""The per-peer globals that moved into ItlAWDL::PeerSlot (include/AWDLPeerSlot.hpp).

Harnesses that compile driver code from before the move (fixtures) or mocks written with the old names
rename them with rename(), so old and new code share one awdlPeer.
"""
import re

MAP = {
 'awdlCoexBaPeer':'addr','awdlCoexBaPeerAdded':'added','awdlCoexBaPeerAccepted':'accepted',
 'awdlCoexBaPeerEpoch':'epoch','awdlCoexBaPeerWidth':'width','awdlCoexBaPeerLastUS':'lastUS',
 'awdlCoexBaRequestsPending':'requests','awdlCoexBaTids':'rxTids','awdlCoexBaTidTeardown':'rxTidTeardown',
 'awdlCoexBaRxIdleTids':'rxIdleTids','awdlCoexBaTidToken':'rxTidToken','awdlCoexBaRxTidLastUS':'rxTidLastUS',
 'awdlCoexBaAnswer':'answer','awdlCoexBaAnswerLength':'answerLength','awdlCoexBaAnswerTid':'answerTid',
 'awdlCoexBaAnswerRetryUS':'answerRetryUS',
 'awdlCoexDelbaWaitTid':'delbaWaitTid','awdlCoexDelbaWaitGen':'delbaWaitGen','awdlCoexDelbaWaitEpoch':'delbaWaitEpoch',
 'awdlCoexDelbaTries':'delbaTries','awdlCoexDelbaAttemptId':'delbaAttemptId','awdlCoexDelbaResult':'delbaResult',
 'awdlCoexDelbaDeadlineUS':'delbaDeadlineUS','awdlCoexDelbaNextTryUS':'delbaNextTryUS',
 'awdlCoexBaTxState':'txState','awdlCoexBaTxQueue':'txQueue','awdlCoexBaTxSeq':'txSeq','awdlCoexBaTxWindow':'txWindow',
 'awdlCoexBaTxToken':'txToken','awdlCoexBaTxBulk':'txBulk','awdlCoexBaTxLastAskUS':'txLastAskUS',
 'awdlCoexBaTxRestUntilUS':'txRestUntilUS','awdlCoexBaTxLastAggUS':'txLastAggUS','awdlCoexBaTxLastNotifUS':'txLastNotifUS',
 'awdlCoexBaQueueAllocPending':'queueAllocPending','awdlCoexBaProbeTried':'probeTried',
 'awdlCoexBaProbeUntilUS':'probeUntilUS','awdlCoexBaLastRxPayloadUS':'lastRxPayloadUS',
 'awdlCoexPeerPause':'pause','awdlCoexPeerBulk':'bulk','awdlCoexResumePending':'resumePending',
 'awdlCoexResumeRetryUS':'resumeRetryUS','awdlCoexResumeFailStreak':'resumeFailStreak',
}

_PATTERN = re.compile(r'\b(' + '|'.join(sorted(MAP, key=len, reverse=True)) + r')\b')
SLOT = ('#include <AWDLPeerSlot.hpp>\n'
        'static ItlAWDL::PeerSlot awdlPeers[ItlAWDL::kMaxPeerSlots];\n'
        'static ItlAWDL::PeerSlot &awdlPeer = awdlPeers[0];\n'
        'static unsigned awdlPeerSlotsUsable = 1;\n'
        'static unsigned awdlPeerSlotFull;\n')


def rename(text):
    return _PATTERN.sub(lambda m: 'awdlPeer.' + MAP[m.group(1)], text)


_REVERSE = re.compile(r'\bawdlPeer\.(' + '|'.join(sorted(MAP.values(), key=len, reverse=True)) + r')\b')
_OLD = {field: old for old, field in MAP.items()}


def unrename(text):
    """The other way, for pins taken before the move: a rename-only change hashes the same."""
    return _REVERSE.sub(lambda m: _OLD[m.group(1)], text)
