#ifndef __VERSION_INFORMATION_H
#define __VERSION_INFORMATION_H

/* Defines */
#define UNIQUE_ID_SIZE          12
#define VERSION_MAX_SIZE        70
#define USER_ID_MAX_SIZE        100

#define UNIQUE_ID_BASE          0x1fff7a10
#define FIRMWARE_BASE           0x08000000
#define BOOTLOADER_BASE         0x08000000
#define SW_VERSION_OFFSET       0x1c0

#ifndef GIT_DATE
    #define GIT_DATE            "no timestamp"
#endif

#ifndef GIT_VERSION
    #define GIT_VERSION         "no version"
#endif

#define KFLY_VERSION    GIT_VERSION ", Build date: " GIT_DATE "\0"

/* Typedefs */

/* Macros */

/* Global functions */
void VersionInformationInit(void);
uint8_t *ptrGetUniqueID(void);
uint8_t *ptrGetBootloaderVersion(void);
uint8_t *ptrGetFirmwareVersion(void);
uint8_t *ptrGetUserIDString(void);

#endif
