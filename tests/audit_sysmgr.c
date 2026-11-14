/* Export the production identity/discovery for optional private-input proof. */
#include "sysmgr.h"
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
EXPORT int audit_sysmgr_identity(const uint64_t *words)
{return sysmgr_entry_identity(words);}
EXPORT int audit_sysmgr_scan(sysmgr_read_fn read,void *user,unsigned *count,sysmgr_report *report)
{sysmgr_page pages[SYSMGR_PAGE_LIMIT];return sysmgr_discover(read,user,pages,count,report);}
