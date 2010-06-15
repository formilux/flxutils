// Macro from 2.6.27.46 kernel

#ifndef _UNALIGNED_H_
#define _UNALIGNED_H_

#include "le_byteshift.h"
#include "be_byteshift.h"
#include "generic.h"

#ifdef __LITTLE_ENDIAN__

# define get_unaligned __get_unaligned_le
# define put_unaligned __put_unaligned_le

#else
# define get_unaligned __get_unaligned_be
# define put_unaligned __put_unaligned_be
#endif 

#endif /* _UNALIGNED_H_ */
