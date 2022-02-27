#pragma once

void bsc_init();
void bsc_reset(bool hard);
void bsc_term();

void cpg_init();
void cpg_reset();
void cpg_term();

void dmac_init();
void dmac_reset();
void dmac_term();

void rtc_init();
void rtc_reset();
void rtc_term();

void intc_init();
void intc_reset();
void intc_term();

void serial_init();
void serial_reset();
void serial_term();

void ubc_init();
void ubc_reset();
void ubc_term();

void tmu_init();
void tmu_reset(bool hard);
void tmu_term();

void ccn_init();
void ccn_reset(bool hard);
void ccn_term();
