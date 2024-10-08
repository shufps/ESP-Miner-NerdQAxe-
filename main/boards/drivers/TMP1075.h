#ifndef TMP1075_H_
#define TMP1075_H_

#define TMP1075_I2CADDR_DEFAULT 0x48 ///< TMP1075 i2c address
#define TMP1075_TEMP_REG 0x00        ///< Temperature register
#define TMP1075_CONFIG_REG 0x01      ///< Configuration register
#define TMP1075_LOW_LIMIT 0x02       ///< Low limit register
#define TMP1075_HIGH_LIMIT 0x03      ///< High limit register
#define TMP1075_DEVICE_ID 0x0F       ///< Device ID register

bool TMP1075_installed(int);
float TMP1075_read_temperature(int);

#endif /* TMP1075_H_ */
