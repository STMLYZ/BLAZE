/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* 0.11 LOOP CAPTURE kept: the loop in the echo's line (blaze_cap.c: 22.05 kHz IMA ADPCM, from step index BZC_IDX0)
 * written into a USR slot as one looped zone (eng_sample.c's slot format, as tools/fm1_sample_upload.py writes it,
 * through editor.c's own steps): the first empty slot, else the first a capture filled before (named CAP...); one
 * holding an upload is never written over. 80 KiB hold 7.4 s: the loop's bars, halved until they fit (4 bars from 130
 * BPM, 2 from 65). The main loop, stopped (the erase pauses the sound ~1 s); the loop is silent meanwhile */
static int bz_cap_save(void)
{
    smp_user_hdr_t *h = (smp_user_hdr_t *)ed_smp_buf;
    const uint8_t *src = (const uint8_t *)dly_buf;
    uint32_t k, bars = bz_cap_got, codes = bz_cap_len, bytes, off, n, bpm;
    uint8_t prev = bz_cap;
    int rc = 0;
    if (!flash_ok)
        return -2;
    for (k = 0; k < SMP_USER_SLOTS && usr_nz[k]; k++)
        ;
    if (k == SMP_USER_SLOTS)
        for (k = 0; k < SMP_USER_SLOTS; k++) {
            const smp_user_hdr_t *o = (const smp_user_hdr_t *)smp_user_xip(k);
            if (o->name[0] == 'C' && o->name[1] == 'A' && o->name[2] == 'P')
                break;
        }
    if (k == SMP_USER_SLOTS)
        return -1;
    while (bars > 1u && (codes + 1u) / 2u > SMP_USER_SIZE - SMP_USER_DATA) {
        bars >>= 1;
        codes >>= 1;
    }
    bytes = (codes + 1u) / 2u;
    if (!bars || !codes || bytes > SMP_USER_SIZE - SMP_USER_DATA)
        return -2;
    bz_cap = BZC_SAVE;                               /* (the ISR leaves the line alone, the loop silent) */
    if (ed_flash_stop() || ed_smp_erase(k, 1))
        rc = -2;
    for (off = 0; !rc && off < bytes; off += 256u) {
        n = bytes - off < 256u ? bytes - off : 256u;
        memcpy(ed_smp_buf, src + off, n);            /* (through the flash buffer, as an upload's data comes) */
        if (fl_write(ed_smp_slot(k) + SMP_USER_DATA + off, ed_smp_buf, n))
            rc = -2;
        fm1_wdt_feed();
    }
    if (!rc) {
        ed_smp_inval(k);
        memset(ed_smp_buf, 0, sizeof ed_smp_buf);
        h->magic = SMP_USER_MAGIC;
        h->version = 1;
        h->nz = 1;
        bpm = beat_samples() ? (uint32_t)FS * 60u / beat_samples() : 0u;
        memcpy(h->name, "CAP", 3);
        h->name[3] = (char)('0' + bars % 10u);
        h->name[4] = 'B';
        h->name[5] = (char)('0' + bpm / 100u % 10u);
        h->name[6] = (char)('0' + bpm / 10u % 10u);
        h->name[7] = (char)('0' + bpm % 10u);
        h->data_len = bytes;
        h->crc = st_crc32(smp_user_xip(k) + SMP_USER_DATA, bytes);
        h->zone[0] = (smp_zone_t){.off = 0, .n = codes, .ls = 0, .le = codes - 1u, .rate = 32768u, .root16 = 60 * 16,
                                  .pred = 0, .idx = BZC_IDX0, .lo = 0, .hi = 127, .looped = 1};
        if (fl_write(ed_smp_slot(k), ed_smp_buf, sizeof(smp_user_hdr_t)))
            rc = -2;
        ed_smp_inval(k);
        smp_user_scan(k);
        if (!rc && !usr_nz[k])
            rc = -2;
    }
    bz_cap = prev;
    return rc ? rc : (int)k + 1;
}
