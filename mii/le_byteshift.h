#ifndef _LINUX_UNALIGNED_LE_BYTESHIFT_H
#define _LINUX_UNALIGNED_LE_BYTESHIFT_H

//#include <linux/kernel.h>

static inline unsigned short __get_unaligned_le16(const unsigned char *p)
{
	return p[0] | p[1] << 8;
}

static inline unsigned int __get_unaligned_le32(const unsigned char *p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
}

static inline long long __get_unaligned_le64(const unsigned char *p)
{
	return (long long)__get_unaligned_le32(p + 4) << 32 |
	       __get_unaligned_le32(p);
}

static inline void __put_unaligned_le16(unsigned short val, unsigned char *p)
{
	*p++ = val;
	*p++ = val >> 8;
}

static inline void __put_unaligned_le32(unsigned int val, unsigned char *p)
{
	__put_unaligned_le16(val >> 16, p + 2);
	__put_unaligned_le16(val, p);
}

static inline void __put_unaligned_le64(long long val, unsigned char *p)
{
	__put_unaligned_le32(val >> 32, p + 4);
	__put_unaligned_le32(val, p);
}

static inline unsigned short get_unaligned_le16(const void *p)
{
	return __get_unaligned_le16((const unsigned char *)p);
}

static inline unsigned int get_unaligned_le32(const void *p)
{
	return __get_unaligned_le32((const unsigned char *)p);
}

static inline long long get_unaligned_le64(const void *p)
{
	return __get_unaligned_le64((const unsigned char *)p);
}

static inline void put_unaligned_le16(unsigned short val, void *p)
{
	__put_unaligned_le16(val, p);
}

static inline void put_unaligned_le32(unsigned int val, void *p)
{
	__put_unaligned_le32(val, p);
}

static inline void put_unaligned_le64(long long val, void *p)
{
	__put_unaligned_le64(val, p);
}

#endif /* _LINUX_UNALIGNED_LE_BYTESHIFT_H */
