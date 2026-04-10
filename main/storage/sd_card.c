#include "sd_card.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include <string.h>

static const char *TAG = "SD_CARD";
static sdmmc_card_t *card = NULL;

esp_err_t sd_card_init(void) {
    esp_err_t ret;

    // 挂载配置
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, // 不自动格式化
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    const char mount_point[] = "/sdcard";
    ESP_LOGI(TAG, "初始化 SD 卡... 挂载点: %s", mount_point);

    // 1. 初始化 SDMMC 主机配置
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    
    // 2. 使用 SDMMC 1-line 模式 (因为原理图只接了 DAT0、CMD、CLK)
    host.flags = SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    // 3. 配置引脚
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = SD_CLK_PIN;
    slot_config.cmd = SD_CMD_PIN;
    slot_config.d0  = SD_DAT0_PIN;

    // 4. 挂载文件系统
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "无法挂载文件系统。如果需要可以在配置里开启格式化。");
        } else {
            ESP_LOGE(TAG, "挂载 SD 卡失败 (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    ESP_LOGI(TAG, "SD 卡已挂载");
    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}

void sd_card_deinit(void) {
    const char mount_point[] = "/sdcard";
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "SD 卡已卸载");
}
