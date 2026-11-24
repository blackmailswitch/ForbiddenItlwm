"""Run the actual AWDL branch of iwx_rs_init (the peer's rate control for send block ack).

#83: with a 160 MHz HE (or VHT 160) station, the HE and VHT rate helpers fill the 160 MHz
entries of the rate table from the BSS node before this branch runs. The branch must leave
no rate for a width the AWDL peer doesn't use, or the firmware asserts on the command.
Only the command fields are simulated: no firmware, radio or macOS.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "itlwm/hal_iwx/ItlIwx.cpp").read_text()


def block(start):
    opening = source.index("{", source.index(start))
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[opening:end]


branch = block("    if (awdlBaTlc) {").replace("goto send;", "return;")
program = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#define htole16(x) (x)
enum { IWX_TLC_NSS_1 = 0, IWX_TLC_NSS_2 = 1, IWX_TLC_NSS_MAX = 2 };
enum { IWX_TLC_MCS_PER_BW_80 = 0, IWX_TLC_MCS_PER_BW_160 = 1, IWX_TLC_MCS_PER_BW_NUM_V4 = 3 };
enum { IWX_TLC_MNG_MODE_NON_HT = 0, IWX_TLC_MNG_MODE_HT = 1, IWX_TLC_MNG_MODE_VHT = 2, IWX_TLC_MNG_MODE_HE = 3 };
enum { IWX_TLC_MNG_CH_WIDTH_20MHZ = 0, IWX_TLC_MNG_CH_WIDTH_80MHZ = 2, IWX_TLC_MNG_CH_WIDTH_160MHZ = 3 };
struct iwx_tlc_config_cmd_v4 {
    uint8_t sta_id, max_ch_width, mode, chains, sgi_ch_width_supp;
    uint16_t flags, non_ht_rates;
    uint16_t ht_rates[IWX_TLC_NSS_MAX][IWX_TLC_MCS_PER_BW_NUM_V4];
};
struct ieee80211_channel { int two_ghz; };
#define IEEE80211_IS_CHAN_2GHZ(c) ((c)->two_ghz)
struct iwx_softc {};
static const uint8_t kAwdlBaStationId = 3;
static uint8_t awdlBaTlcStation = kAwdlBaStationId;
static ieee80211_channel *awdlCoexOwnChan;
static bool awdlBaTx80Active;
static unsigned awdlBaTxMinMcs = 4;
static uint8_t iwx_rs_fw_set_active_chains(uint8_t ant) { return ant; }
static uint8_t iwx_fw_valid_tx_ant(iwx_softc *) { return 3; }
static uint16_t iwx_rs_fw_get_config_flags(iwx_softc *) { return 0; }
static void awdl_branch(iwx_softc *sc, iwx_tlc_config_cmd_v4 &cfg_cmd)
''' + branch + r'''
static void fill_from_he160(iwx_tlc_config_cmd_v4 &cfg) {
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = IWX_TLC_MNG_MODE_HE;
    for (int ss = 0; ss < IWX_TLC_NSS_MAX; ++ss)
        for (int bw = 0; bw < 2; ++bw)
            cfg.ht_rates[ss][bw] = 0x0fff;   // as logged in #83: rates80 and rates160 = 0fff/0fff
}
int main() {
    iwx_softc sc;
    ieee80211_channel ch149{0};
    awdlCoexOwnChan = &ch149;
    for (int wide = 0; wide < 2; ++wide) {
        awdlBaTx80Active = wide;
        iwx_tlc_config_cmd_v4 cfg;
        fill_from_he160(cfg);
        awdl_branch(&sc, cfg);
        assert(cfg.sta_id == kAwdlBaStationId);
        assert(cfg.mode == (wide ? IWX_TLC_MNG_MODE_VHT : IWX_TLC_MNG_MODE_HT));
        for (int ss = 0; ss < IWX_TLC_NSS_MAX; ++ss)
            for (int bw = IWX_TLC_MCS_PER_BW_160; bw < IWX_TLC_MCS_PER_BW_NUM_V4; ++bw)
                assert(cfg.ht_rates[ss][bw] == 0);
        const uint16_t expected80 = wide ? uint16_t(0x3ffU & ~((1U << awdlBaTxMinMcs) - 1U)) : 0xf0;
        assert(cfg.ht_rates[IWX_TLC_NSS_1][IWX_TLC_MCS_PER_BW_80] == expected80);
        assert(cfg.ht_rates[IWX_TLC_NSS_2][IWX_TLC_MCS_PER_BW_80] == expected80);
        assert(cfg.max_ch_width == (wide ? IWX_TLC_MNG_CH_WIDTH_80MHZ : IWX_TLC_MNG_CH_WIDTH_20MHZ));
    }
    puts("AWDL peer rate control: no 160 MHz rates left from an HE 160 MHz station, HT and VHT 80 (#83): passed (host fixture)");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="awdl-tlc-") as directory:
    src = Path(directory) / "test.cpp"
    exe = Path(directory) / "test"
    src.write_text(program)
    subprocess.run(["xcrun", "clang++", "-std=c++14", "-fsanitize=address,undefined", str(src), "-o", str(exe)],
                   check=True)
    subprocess.run([str(exe)], check=True)
