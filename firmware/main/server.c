#ifdef DEBUG_WIFI

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include <string.h>
#include "server.h"

static const char *TAG = "SRV_UDP";
static volatile int _srv_fd = -1;
static struct sockaddr_in _client_addr;
static volatile bool _client_connected = false;

static void _server_task(void *arg) {
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(SRV_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    int srv = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (srv < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket");
        vTaskDelete(NULL);
        return;
    }

    bind(srv, (struct sockaddr *)&addr, sizeof(addr));
    _srv_fd = srv;
    ESP_LOGI(TAG, "Listening for UDP PING on port %d", SRV_PORT);

    char rx_buf[32];
    while (1) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        
        // Block until receiving a registration "PING" datagram from Python client
        int len = recvfrom(srv, rx_buf, sizeof(rx_buf) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len > 0) {
            _client_addr = source_addr;
            _client_connected = true;
            ESP_LOGI(TAG, "Client connected via UDP. Starting data stream.");
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void server_init(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid           = SRV_SSID,
            .password       = SRV_PASS,
            .ssid_len       = strlen(SRV_SSID),
            .channel        = 11,
            .authmode       = WIFI_AUTH_WPA2_PSK,
            .max_connection = 1,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();

    ESP_LOGI(TAG, "AP Started: SSID=%s IP=192.168.4.1", SRV_SSID);

    xTaskCreate(_server_task, "srv_task", 4096, NULL, 5, NULL);
}

bool server_send(dbg_packet_t *pkt) {
    if (!_client_connected || _srv_fd < 0) return false;

    pkt->magic = SRV_MAGIC;
    int ret = sendto(_srv_fd, pkt, sizeof(dbg_packet_t), MSG_DONTWAIT, (struct sockaddr *)&_client_addr, sizeof(_client_addr));
    
    return (ret == (int)sizeof(dbg_packet_t));
}

#endif
