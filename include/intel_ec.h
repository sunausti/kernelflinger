#ifndef _INTEL_EC_
#define _INTEL_EC_

UINT8 get_ec_sub_ver(UINT8 Port);
void output_ec_version(void);
int update_ec(void *data, uint32_t len);

#endif

