#ifdef DEBUG_WIFI

#include "tcp_debug.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include <string.h>

static const char *TAG = "WDBG";
static volatile int _client_fd = -1;


static void _server_task(void *arg) {
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(DBG_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    int srv = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(srv, (struct sockaddr *)&addr, sizeof(addr));
    listen(srv, 1);
    ESP_LOGI(TAG, "aguardando cliente na porta %d", DBG_PORT);

    while (1) {
        int fd = accept(srv, NULL, NULL);
        if (fd < 0) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }

        ESP_LOGI(TAG, "cliente conectado");
        _client_fd = fd;

        while (_client_fd >= 0)
            vTaskDelay(pdMS_TO_TICKS(100));

        close(fd);
        ESP_LOGI(TAG, "cliente desconectado, aguardando reconexão");
    }
}


void wifi_debug_init(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid            = DBG_SSID,
            .password        = DBG_PASS,
            .ssid_len        = strlen(DBG_SSID),
            .channel         = 11,
            .authmode        = WIFI_AUTH_WPA2_PSK,
            .max_connection  = 1,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();

    ESP_LOGI(TAG, "AP iniciado: SSID=%s IP=192.168.4.1", DBG_SSID);

    xTaskCreate(_server_task, "wdbg_srv", 4096, NULL, 5, NULL);
}

bool wifi_debug_send(dbg_packet_t *pkt) {
    int fd = _client_fd;
    if (fd < 0) return false;

    pkt->magic = DBG_MAGIC;
    int ret = send(fd, pkt, sizeof(dbg_packet_t), MSG_DONTWAIT);
    if (ret != (int)sizeof(dbg_packet_t)) {
        _client_fd = -1; 
        return false;
    }
    return true;
}

#endif
