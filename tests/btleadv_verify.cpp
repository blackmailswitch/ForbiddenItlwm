// BTLEAdvVerify.hpp: the block APFS checks after a partial reload of bluetoothd, as decmpfs builds it (#62).
#include <BTLEAdvVerify.hpp>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace BTLEAdv;
static const size_t kBlock = 4 * kVerifyPage;   // APFS's 16 KB hash block
static uint64_t fnv(const uint8_t *p, size_t n) {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}
static PatchedPages pages;   // large: not on the stack
int main() {
    // A block at 0x1e4000 of the file, as on disk.
    const uint64_t base = 0x1e4000;
    std::vector<uint8_t> disk(kBlock);
    for (size_t i = 0; i < kBlock; i++) disk[i] = uint8_t(i * 7 + 3);
    const uint64_t want = fnv(disk.data(), kBlock);

    // Its last page is validated and patched in memory (BlueToolFixup's 8-byte NOP at 0x1e7737).
    std::vector<uint8_t> page3(disk.begin() + 3 * kVerifyPage, disk.end());
    std::vector<uint8_t> patched3 = page3;
    memset(patched3.data() + 0x737, 0x90, 8);
    assert(recordPatchedPage(pages, base + 3 * kVerifyPage, page3.data(), patched3.data()));
    assert(pages.count == 1);
    // A page no kext changed isn't recorded, nor one at an unaligned offset.
    assert(!recordPatchedPage(pages, base, disk.data(), disk.data()));
    assert(!recordPatchedPage(pages, base + 1, page3.data(), patched3.data()));
    assert(pages.count == 1);

    // Page 2 is dropped and reloaded: decmpfs decompresses it and leaves pages 0, 1 and 3 as they are in memory.
    std::vector<uint8_t> buffer = disk;
    memcpy(buffer.data() + 3 * kVerifyPage, patched3.data(), kVerifyPage);
    assert(fnv(buffer.data(), kBlock) != want);                   // the failure in #62
    assert(holdsPatchedBytes(pages, base, buffer.data(), kBlock));
    std::vector<uint8_t> copy = buffer;
    assert(restoreOriginalBytes(pages, base, copy.data(), kBlock) == 1);
    assert(fnv(copy.data(), kBlock) == want);                     // the check now passes
    assert(!memcmp(buffer.data() + 3 * kVerifyPage, patched3.data(), kVerifyPage));   // the buffer itself is untouched

    // The patched page itself was dropped too, so the whole block is fresh from disk: nothing to do.
    assert(!holdsPatchedBytes(pages, base, disk.data(), kBlock));
    // A page that is neither the original nor the recorded patch is left for APFS to refuse.
    std::vector<uint8_t> other = buffer;
    other[3 * kVerifyPage + 5] ^= 0xff;
    assert(!holdsPatchedBytes(pages, base, other.data(), kBlock));
    copy = other;
    assert(restoreOriginalBytes(pages, base, copy.data(), kBlock) == 0);
    // A block that doesn't contain the page, and one that only partly does.
    assert(!holdsPatchedBytes(pages, base + kBlock, disk.data(), kBlock));
    // (the last half of page 2, then the first half of page 3, which holds the patch at 0x737)
    std::vector<uint8_t> half(kVerifyPage);
    memcpy(half.data(), disk.data() + 2 * kVerifyPage + kVerifyPage / 2, kVerifyPage / 2);
    memcpy(half.data() + kVerifyPage / 2, patched3.data(), kVerifyPage / 2);
    const uint64_t halfStart = base + 2 * kVerifyPage + kVerifyPage / 2;
    assert(holdsPatchedBytes(pages, halfStart, half.data(), kVerifyPage));
    assert(restoreOriginalBytes(pages, halfStart, half.data(), kVerifyPage) == 1);
    assert(!memcmp(half.data() + kVerifyPage / 2, page3.data(), kVerifyPage / 2));
    assert(!memcmp(half.data(), disk.data() + 2 * kVerifyPage + kVerifyPage / 2, kVerifyPage / 2));
    // A null buffer (only the block size is being asked for).
    assert(!holdsPatchedBytes(pages, base, nullptr, 0));

    // The page is patched again after a reload: its record is replaced, not added.
    memset(patched3.data() + 0x100, 0xcc, 2);
    assert(recordPatchedPage(pages, base + 3 * kVerifyPage, page3.data(), patched3.data()));
    assert(pages.count == 1);
    // Room for twelve pages; the next is counted, not recorded.
    for (size_t i = 1; i < kMaxPatchedPages; i++)
        assert(recordPatchedPage(pages, 0x100000 + i * kVerifyPage, page3.data(), patched3.data()));
    assert(pages.count == kMaxPatchedPages);
    assert(!recordPatchedPage(pages, 0x900000, page3.data(), patched3.data()) && pages.full == 1);
    puts("btleadv_verify: passed");
    return 0;
}
