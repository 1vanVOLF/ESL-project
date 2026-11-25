#include "color_storage.h"
#include <string.h>
#include "nrfx_nvmc.h"

#define NRF_LOG_MODULE_NAME cstor
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
NRF_LOG_MODULE_REGISTER();

#define ERASED_WORD   0xFFFFFFFFUL
#define NVMC_WAIT_MAX_ITERATIONS   1000000UL

static bool wait_for_nvmc_ready(void)
{
    for (uint32_t i = 0; i < NVMC_WAIT_MAX_ITERATIONS; i++)
    {
        if (nrfx_nvmc_write_done_check())
        {
            return true;
        }
    }
    return false;
}

_Static_assert(sizeof(hsv_t) == sizeof(uint32_t), "hsv_t must pack into exactly one flash word");

static uint32_t m_next_free_addr = COLOR_STORAGE_START_ADDR;

static bool slot_is_erased(uint32_t word)
{
    return (word == ERASED_WORD);
}

static bool slot_holds_sane_color(hsv_t const *p_hsv)
{
    return (p_hsv->hue <= HUE_MAX) &&
           (p_hsv->saturation <= PERCENT_MAX) &&
           (p_hsv->value <= PERCENT_MAX);
}

static void erase_storage_area(void)
{
    NRF_LOG_WARNING("Storage area full - erasing %d page(s)", COLOR_STORAGE_PAGE_COUNT);

    for (uint32_t page = 0; page < COLOR_STORAGE_PAGE_COUNT; page++)
    {
        uint32_t page_addr = COLOR_STORAGE_START_ADDR + (page * COLOR_STORAGE_PAGE_SIZE);
        nrfx_nvmc_page_erase(page_addr);
        if (!wait_for_nvmc_ready())
        {
            NRF_LOG_ERROR("NVMC erase timed out at 0x%X", page_addr);
            return;
        }
    }

    m_next_free_addr = COLOR_STORAGE_START_ADDR;
}

bool color_storage_load(hsv_t *p_hsv)
{
    bool found_any = false;
    hsv_t latest = {0};
    uint32_t next_free = COLOR_STORAGE_START_ADDR;

    for (uint32_t addr = COLOR_STORAGE_START_ADDR; addr < COLOR_STORAGE_END_ADDR; addr += sizeof(uint32_t))
    {
        uint32_t raw = *(volatile uint32_t *)addr;

        if (slot_is_erased(raw))
        {
            next_free = addr;
            break;
        }

        hsv_t candidate;
        memcpy(&candidate, &raw, sizeof(candidate));

        if (slot_holds_sane_color(&candidate))
        {
            latest = candidate;
            found_any = true;
            next_free = addr + sizeof(uint32_t);
        }
        else
        {
            NRF_LOG_WARNING("Unexpected data at 0x%X - ignoring saved color", addr);
            found_any = false;
            break;
        }
    }

    m_next_free_addr = next_free;

    if (found_any)
    {
        *p_hsv = latest;
        NRF_LOG_INFO("Loaded saved color: H=%d S=%d V=%d", latest.hue, latest.saturation, latest.value);
    }
    else
    {
        NRF_LOG_INFO("No saved color found, caller will use the default");
    }

    return found_any;
}

void color_storage_save(hsv_t hsv)
{
    uint32_t word;
    memcpy(&word, &hsv, sizeof(word));

    if (m_next_free_addr >= COLOR_STORAGE_END_ADDR)
    {
        erase_storage_area();
    }

    if (!nrfx_nvmc_word_writable_check(m_next_free_addr, word))
    {
        erase_storage_area();
    }

    nrfx_nvmc_word_write(m_next_free_addr, word);
    if (!wait_for_nvmc_ready())
    {
        NRF_LOG_ERROR("NVMC write timed out at 0x%X", m_next_free_addr);
        return;
    }

    NRF_LOG_INFO("Saved color H=%d S=%d V=%d at 0x%X", hsv.hue, hsv.saturation, hsv.value, m_next_free_addr);

    m_next_free_addr += sizeof(uint32_t);
}
