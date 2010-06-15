#ifndef _LINUX_UNALIGNED_BE_BYTESHIFT_H
#define _LINUX_UNALIGNED_BE_BYTESHIFT_H

//#include <linux/kernel.h>

static inline unsigned short __get_unaligned_be16(const unsigned char *p)
{
	return p[0] << 8 | p[1];
}

static inline unsigned int __get_unaligned_be32(const unsigned char *p)
{
	return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

static inline long long __get_unaligned_be64(const unsigned char *p)
{
	return (long long)__get_unaligned_be32(p) << 32 |
	       __get_unaligned_be32(p + 4);
}

static inline void __put_unaligned_be16(unsigned short val, unsigned char *p)
{
	*p++ = val >> 8;
	*p++ = val;
}

static inline void __put_unaligned_be32(unsigned int val, unsigned char *p)
{
	__put_unaligned_be16(val >> 16, p);
	__put_unaligned_be16(val, p + 2);
}

static inline void __put_unaligned_be64(long long val, unsigned char *p)
{
	__put_unaligned_be32(val >> 32, p);
	__put_unaligned_be32(val, p + 4);
}

static inline unsigned short get_unaligned_be16(const void *p)
{
	return __get_unaligned_be16((const unsigned char *)p);
}

static inline unsigned int get_unaligned_be32(const void *p)
{
	return __get_unaligned_be32((const unsigned char *)p);
}

static inline long long get_unaligned_be64(const void *p)
{
	return __get_unaligned_be64((const unsigned char *)p);
}

static inline void put_unaligned_be16(unsigned short val, void *p)
{
	__put_unaligned_be16(val, p);
}

static inline void put_unaligned_be32(unsigned int val, void *p)
{
	__put_unaligned_be32(val, p);
}

static inline void put_unaligned_be64(long long val, void *p)
{
	__put_unaligned_be64(val, p);
}

#endif /* _LINUX_UNALIGNED_BE_BYTESHIFT_H */
