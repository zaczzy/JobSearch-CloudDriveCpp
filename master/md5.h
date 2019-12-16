#ifndef MD5_H_
#define MD5_H_
#define MD5_DIGEST_LENGTH 16
void computeDigest(char *data, int dataLengthBytes, unsigned char *digestBuffer);
#endif