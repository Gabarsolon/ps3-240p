/* Read-only discovery of the supported 4.93 service. No console transport. */
#ifndef CRT_SYSMGR_H
#define CRT_SYSMGR_H
#include <stddef.h>
#include <stdint.h>
#define SYSMGR_PAGE_LIMIT 512
#define SYSMGR_RAM_LIMIT UINT64_C(0x02000000)
typedef uint64_t (*sysmgr_read_fn)(void *,uint64_t);
typedef struct {uint64_t physical,size;uint32_t virtual_address;} sysmgr_page;
typedef struct {
 unsigned pid,occupied,domains,matches;
 uint64_t process,domain,slot10_pointer;
 uint32_t slot10_pid;
 char error[128];
} sysmgr_report;
/* Output count is zero on every refusal. Each call scans all 32 slots, checks
 * uniqueness, then rechecks the selected mapping and table before returning.
 * Callers still own all target-byte preflight and checked-write rollback. */
int sysmgr_discover(sysmgr_read_fn,void *,sysmgr_page *,unsigned *,sysmgr_report *);
int sysmgr_translate(const sysmgr_page *,unsigned,uint32_t,uint64_t *);
/* Immutable 32-byte entry identity; hashes only, never embedded Sony code. */
int sysmgr_entry_identity(const uint64_t words[4]);
#endif
