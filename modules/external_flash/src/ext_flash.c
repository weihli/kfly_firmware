/* *
 *
 * Hardware Abstraction Layer for External Flash
 *
 * */

#include "ch.h"
#include "hal.h"
#include "ext_flash.h"
#include "sensor_read.h"
#include "control.h"
#include "rc_input.h"

/* Global variable defines */

/* Private variable defines */

/* Private function defines */
static void ExternalFlash_WaitForWriteEnd(const ExternalFlashConfig *config,
                                          uint32_t delay_ms);
static void ExternalFlash_WriteEnable(const ExternalFlashConfig *config);

/* Private external functions */

/**
 * @brief               Initializes the External Flash and checks
 *                      for correct ID.
 *
 * @param[in] config    Pointer to External Flash config.
 */
void ExternalFlashInit(const ExternalFlashConfig *config)
{
    uint32_t id;

    chMtxObjectInit(&config->data->flash_mutex);

    id = ExternalFlash_ReadID(config);

    if (id != config->jedec_id)
        chSysHalt("External Flash ID error.");
}

/**
 * @brief               Erases the entire External Flash.
 *
 * @param[in] config    Pointer to External Flash config.
 */
void ExternalFlash_EraseBulk(const ExternalFlashConfig *config)
{
    /* Claim external flash */
    ExternalFlash_Claim(config);

    /* Enable the write access to the External Flash */
    ExternalFlash_WriteEnable(config);

    /* Claim the SPI bus */
    spiAcquireBus(config->spip);

    /* Select the External Flash: Chip Select low */
    ExternalFlash_Select(config);

    /* Send Bulk Erase instruction  */
    spiPolledExchange(config->spip, FLASH_CMD_BE);

    /* Deselect the External Flash: Chip Select high */
    ExternalFlash_Unselect(config);

    /* Release the SPI bus */
    spiReleaseBus(config->spip);

    /* Wait the end of Flash writing */
    ExternalFlash_WaitForWriteEnd(config, 100);

    /* Release external flash */
    ExternalFlash_Release(config);
}

/**
 * @brief               Erases a sector on the External Flash.
 *
 * @param[in] config    Pointer to External Flash config.
 * @param[in] address   Address to the flash sector.
 */
void ExternalFlash_EraseSector(const ExternalFlashConfig *config,
                               uint32_t address)
{
    /* Claim external flash */
    ExternalFlash_Claim(config);

    /* Enable the write access to the External Flash */
    ExternalFlash_WriteEnable(config);

    /* Claim the SPI bus */
    spiAcquireBus(config->spip);

    /* Select the External Flash: Chip Select low */
    ExternalFlash_Select(config);

    /* Send Sector Erase instruction  */
    spiPolledExchange(config->spip, FLASH_CMD_SE);
    
    /* Send address high nibbles */
    spiPolledExchange(config->spip, (address & 0xFF0000) >> 16);
    spiPolledExchange(config->spip, (address & 0xFF00) >> 8);
    spiPolledExchange(config->spip, address & 0xFF);
    
    /* Deselect the External Flash: Chip Select high */
    ExternalFlash_Unselect(config);

    /* Release the SPI bus */
    spiReleaseBus(config->spip);

    /* Wait the end of Flash writing */
    ExternalFlash_WaitForWriteEnd(config, 100);

    /* Release external flash */
    ExternalFlash_Release(config);
}

/**
 * @brief               Erases a page on the External Flash.
 *
 * @param[in] config    Pointer to External Flash config.
 * @param[in] address   Address to the flash page.
 */
void ExternalFlash_ErasePage(const ExternalFlashConfig *config,
                             uint32_t address)
{
    /* Claim external flash */
    ExternalFlash_Claim(config);

    /* Enable the write access to the External Flash */
    ExternalFlash_WriteEnable(config);

    /* Claim the SPI bus */
    spiAcquireBus(config->spip);

    /* Select the External Flash: Chip Select low */
    ExternalFlash_Select(config);

    /* Send Page Erase instruction  */
    spiPolledExchange(config->spip, FLASH_CMD_PE);
    
    /* Send address high nibbles */
    spiPolledExchange(config->spip, (address & 0xFF0000) >> 16);
    spiPolledExchange(config->spip, (address & 0xFF00) >> 8);
    spiPolledExchange(config->spip, address & 0xFF);
    
    /* Deselect the External Flash: Chip Select high */
    ExternalFlash_Unselect(config);

    /* Release the SPI bus */
    spiReleaseBus(config->spip);

    /* Wait the end of Flash writing */
    ExternalFlash_WaitForWriteEnd(config, 10);

    /* Release external flash */
    ExternalFlash_Release(config);
}

/**
 * @brief               Gets the ID of the External Flash.
 *
 * @param[in] config    Pointer to External Flash config.
 * @return              External Flash ID.
 */
uint32_t ExternalFlash_ReadID(const ExternalFlashConfig *config)
{
    uint32_t t1, t2, t3;

    /* Claim external flash */
    ExternalFlash_Claim(config);

    /* Claim the SPI bus */
    spiAcquireBus(config->spip);

    /* Select the External Flash: Chip Select low */
    ExternalFlash_Select(config);

    /* Send "Read ID" instruction */
    spiPolledExchange(config->spip, FLASH_CMD_RDID);

    /* Read the three ID bytes from the External Flash */
    t1 = spiPolledExchange(config->spip, FLASH_DUMMY_BYTE);
    t2 = spiPolledExchange(config->spip, FLASH_DUMMY_BYTE);
    t3 = spiPolledExchange(config->spip, FLASH_DUMMY_BYTE);

    /* Deselect the External Flash: Chip Select high */
    ExternalFlash_Unselect(config);

    /* Release the SPI bus */
    spiReleaseBus(config->spip);

    /* Release external flash */
    ExternalFlash_Release(config);

    return ((t1 << 16) | (t2 << 8) | t3);
}

/**
 * @brief               Writes data to a Flash page using polling.
 * 
 * @param[in] config    Pointer to External Flash config.
 * @param[in] buffer    Pointer to the buffer holding the data.
 * @param[in] address   Where in the Flash to save the data.
 * @param[in] count     Number of bytes to write (max 256 bytes).
 */
void ExternalFlash_WritePagePolling(const ExternalFlashConfig *config,
                                    uint8_t *buffer,
                                    uint32_t address, 
                                    uint16_t count)
{
    /* Error check. */
    if (count >= config->num_pages)
        chSysHalt("Page write size too big");

    /* Claim external flash */
    ExternalFlash_Claim(config);

    /* Enable the write access to the External Flash */
    ExternalFlash_WriteEnable(config);

    /* Claim the SPI bus */
    spiAcquireBus(config->spip);

    /* Select the External Flash: Chip Select low */
    ExternalFlash_Select(config);

    /* Send "Write to Memory" instruction */
    spiPolledExchange(config->spip, FLASH_CMD_PAGE_PROGRAM);

    /* Send address nibbles for address byte to write to */
    spiPolledExchange(config->spip, (address & 0xFF0000) >> 16);  
    spiPolledExchange(config->spip, (address & 0xFF00) >> 8);
    spiPolledExchange(config->spip, address & 0xFF);

    /* While there is data to be written on the External Flash */
    while (count--)
        spiPolledExchange(config->spip, *(buffer++));

    /* Deselect the External Flash: Chip Select high */
    ExternalFlash_Unselect(config);

    /* Release the SPI bus */
    spiReleaseBus(config->spip);

    /* Wait the end of Flash writing */
    ExternalFlash_WaitForWriteEnd(config, 1);

    /* Release external flash */
    ExternalFlash_Release(config);
}

/**
 * @brief               Writes data to a Flash page using DMA.
 * 
 * @param[in] config    Pointer to External Flash config.
 * @param[in] buffer    Pointer to the buffer holding the data.
 * @param[in] address   Where in the Flash to save the data.
 * @param[in] count     Number of bytes to write (max 256 bytes).
 */
void ExternalFlash_WritePage(const ExternalFlashConfig *config,
                             uint8_t *buffer,
                             uint32_t address, 
                             uint16_t count)
{
    /* Error check. */
    if (count >= config->num_pages)
        chSysHalt("Page write size too big");

    /* Claim external flash */
    ExternalFlash_Claim(config);

    /* Enable the write access to the External Flash */
    ExternalFlash_WriteEnable(config);

    /* Claim the SPI bus */
    spiAcquireBus(config->spip);

    /* Select the External Flash: Chip Select low */
    ExternalFlash_Select(config);

    /* Load the data */
    config->data->flash_tmp[0] = FLASH_CMD_PAGE_PROGRAM;
    config->data->flash_tmp[1] = (address & 0xFF0000) >> 16;
    config->data->flash_tmp[2] = (address & 0xFF00) >> 8;
    config->data->flash_tmp[3] = (address & 0xFF);

    /* Send "Write to Memory" instruction and send address nibbles
       from address to read from */
    spiSend(config->spip, 4, config->data->flash_tmp);

    /* Send the data to memory */
    spiSend(config->spip, count, buffer);

    /* Deselect the External Flash: Chip Select high */
    ExternalFlash_Unselect(config);

    /* Release the SPI bus */
    spiReleaseBus(config->spip);

    /* Wait the end of Flash writing */
    ExternalFlash_WaitForWriteEnd(config, 1);

    /* Release external flash */
    ExternalFlash_Release(config);
}

/**
 * @brief               Read a block of data from the External Flash using DMA.
 * 
 * @param[in] config    Pointer to External Flash config.
 * @param[in] buffer    Pointer to the buffer saving the data.
 * @param[in] address   Where in the Flash to read the data.
 * @param[in] count     Number of bytes to read.
 */
void ExternalFlash_ReadBufferPolling(const ExternalFlashConfig *config,
                                     uint8_t *buffer, 
                                     uint32_t address, 
                                     uint16_t count)
{
    /* Claim external flash */
    ExternalFlash_Claim(config);

    /* Claim the SPI bus */
    spiAcquireBus(config->spip);

    /* Select the External Flash: Chip Select low */
    ExternalFlash_Select(config);

    /* Send "Read from Memory" instruction */
    spiPolledExchange(config->spip, FLASH_CMD_READ);

    /* Send address nibbles for address byte to read from */
    spiPolledExchange(config->spip, (address & 0xFF0000) >> 16);
    spiPolledExchange(config->spip, (address & 0xFF00) >> 8);
    spiPolledExchange(config->spip, address & 0xFF);

    while (count--)
        *(buffer++) = spiPolledExchange(config->spip, FLASH_DUMMY_BYTE);

    /* Deselect the External Flash: Chip Select high */
    ExternalFlash_Unselect(config);

    /* Release the SPI bus */
    spiReleaseBus(config->spip);

    /* Release external flash */
    ExternalFlash_Release(config);
}

/**
 * @brief               Read a block of data from the External Flash using DMA.
 * 
 * @param[in] config    Pointer to External Flash config.
 * @param[in] buffer    Pointer to the buffer saving the data.
 * @param[in] address   Where in the Flash to read the data.
 * @param[in] count     Number of bytes to read.
 */
void ExternalFlash_ReadBuffer(const ExternalFlashConfig *config,
                              uint8_t *buffer, 
                              uint32_t address, 
                              uint16_t count)
{
    
    /* Claim external flash */
    ExternalFlash_Claim(config);

    /* Claim the SPI bus */
    spiAcquireBus(config->spip);

    /* Select the External Flash: Chip Select low */
    ExternalFlash_Select(config);

    /* Load the data */
    config->data->flash_tmp[0] = FLASH_CMD_READ;
    config->data->flash_tmp[1] = (address & 0xFF0000) >> 16;
    config->data->flash_tmp[2] = (address & 0xFF00) >> 8;
    config->data->flash_tmp[3] = (address & 0xFF);

    /* Send "Read from Memory" instruction and send address nibbles
       from address to read from */
    spiSend(config->spip, 4, config->data->flash_tmp);

    /* Read the requested data from memory */
    spiReceive(config->spip, count, buffer);

    /* Deselect the External Flash: Chip Select high */
    ExternalFlash_Unselect(config);

    /* Release the SPI bus */
    spiReleaseBus(config->spip);

    /* Release external flash */
    ExternalFlash_Release(config);
}

/**
 * @brief               Polls the status of the Write In Progress (WIP) flag in
 *                      the External Flash's status register until write
 *                      operation has completed.
 * 
 * @param[in] config    Pointer to External Flash config.
 * @param[in] delay_ms  Delay between checks in ms. 0 for continous polling.
 */
static void ExternalFlash_WaitForWriteEnd(const ExternalFlashConfig *config,
                                          uint32_t delay_ms)
{
    uint8_t wip;

    /* Loop as long as the memory is busy with a write cycle */
    do
    {
        /* If a delay was specified: wait */
        if (delay_ms != 0)
            chThdSleep(OSAL_MS2ST(delay_ms));

        /* Claim the SPI bus */
        spiAcquireBus(config->spip);

        /* Select the External Flash: Chip Select low */
        ExternalFlash_Select(config);

        /* Send "Read Status Register" instruction */
        spiPolledExchange(config->spip, FLASH_CMD_RDSR);

        wip = spiPolledExchange(config->spip, FLASH_DUMMY_BYTE);

        /* Deselect the External Flash: Chip Select high */
        ExternalFlash_Unselect(config);

        /* Release the SPI bus */
        spiReleaseBus(config->spip); 
    } while (wip & FLASH_WIP_FLAG);
}

/**
 * @brief               Enables the write access to the External Flash.
 * 
 * @param[in] config    Pointer to External Flash config.
 */
static void ExternalFlash_WriteEnable(const ExternalFlashConfig *config)
{
    /* Claim the SPI bus */
    spiAcquireBus(config->spip);

    /* Select the External Flash: Chip Select low */
    ExternalFlash_Select(config);

    /* Send "Write Enable" instruction */
    spiPolledExchange(config->spip, FLASH_CMD_WREN);

    /* Deselect the External Flash: Chip Select high */
    ExternalFlash_Unselect(config);

    /* Release the SPI bus */
    spiReleaseBus(config->spip);
}