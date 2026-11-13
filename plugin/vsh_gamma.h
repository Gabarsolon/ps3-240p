/* SPDX-License-Identifier: MIT. Experimental native AV gamma, 4.93 CEX only. */
#ifndef VSH_GAMMA_H
#define VSH_GAMMA_H
int vsh_gamma_bind(void);
/* Only the analog SD head is touched. Save/restore its actual three exponents. */
int vsh_gamma_snapshot(float original[3]);
int vsh_gamma_apply(const float exponents[3]);
int vsh_gamma_set(unsigned hundredths);
#endif
