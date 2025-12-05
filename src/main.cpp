#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

static const char *TAG = "UDDI";

// Motor PWM configuration
#define MOTOR_PWM_GPIO    GPIO_NUM_2
#define MOTOR_PWM_CHANNEL LEDC_CHANNEL_0
#define MOTOR_PWM_TIMER   LEDC_TIMER_0

// ESC Protocol types
typedef enum {
    PROTOCOL_STANDARD,   // Standard PWM: 50Hz, 1-2ms pulses
    PROTOCOL_ONESHOT125, // OneShot125: 125-250µs pulses at motor update rate
    PROTOCOL_ONESHOT42,  // OneShot42: 42-84µs pulses
    PROTOCOL_MULTISHOT   // Multishot: 5-25µs pulses
} esc_protocol_t;

static esc_protocol_t current_protocol = PROTOCOL_STANDARD;
static uint32_t pwm_frequency = 50;
static ledc_timer_bit_t pwm_resolution = LEDC_TIMER_14_BIT;
static uint32_t pwm_min_duty = 820;   // Will be updated based on protocol
static uint32_t pwm_max_duty = 1638;  // Will be updated based on protocol

// HTML content for the web interface with slider and protocol dropdown
// GZIP compressed HTML (1928 bytes, saves 4346 bytes from original 6274 bytes)
static const uint8_t html_page_gz[] = {
    0x1f, 0x8b, 0x08, 0x08, 0x2b, 0x23, 0x33, 0x69, 0x02, 0x03, 0x77, 0x65, 0xdd, 0x8e, 0xdb, 0xc6, 0x15, 0x7e, 0x95, 0xb1, 0x8d, 0x62, 0x48, 0xac, 0x44, 0x49, 0x9b, 0xd8, 0xd8, 0x92, 0x22, 0x8b, 0x78, 0xed, 0x45, 0xb7, 0x88, 0xe3, 0x45, 0x76, 0x9d, 0xa2, 0x28, 0x7a, 0x31, 0x22, 0x87, 0xe2, 0xd4, 0xe4, 0x0c, 0x31, 0x1c, 0xad, 0x76, 0x23, 0xe8, 0x09, 0x72, 0x91, 0x8b, 0xf4, 0xaa, 0x2d, 0x90, 0xe6, 0x11, 0x7a, 0x53, 0xf4, 0x79, 0xf2, 0x02, 0xcd, 0x23, 0xf4, 0xcc, 0x0f, 0x25, 0x52, 0x3f, 0xeb, 0x8d, 0x8b, 0x22, 0x16, 0xac, 0xe5, 0x0c, 0xe7, 0x9c, 0x39, 0xe7, 0x9b, 0xef, 0xfc, 0x8c, 0xa6, 0x4f, 0x5e, 0xbd, 0x3d, 0xbf, 0xf9, 0xc3, 0xd5, 0x6b, 0x54, 0xa8, 0xaa, 0x4c, 0xa6, 0xee, 0x9b, 0x92, 0x2c, 0x99, 0x56, 0x54, 0x11, 0x94, 0x16, 0x44, 0x36, 0x54, 0xc5, 0xf8, 0xdd, 0xcd, 0xc5, 0xf0, 0x0c, 0xbb, 0x59, 0x4e, 0x2a, 0x1a, 0xe3, 0x5b, 0x46, 0x97, 0xb5, 0x90, 0x0a, 0xa3, 0x54, 0x70, 0x45, 0x39, 0xac, 0x5a, 0xb2, 0x4c, 0x15, 0x71, 0x46, 0x6f, 0x59, 0x4a, 0x87, 0x66, 0x30, 0x40, 0x8c, 0x33, 0xc5, 0x48, 0x39, 0x6c, 0x52, 0x52, 0xd2, 0x78, 0x12, 0x8c, 0x41, 0x8b, 0x62, 0xaa, 0xa4, 0xc9, 0xbb, 0xe0, 0x15, 0x7c, 0x2e, 0xd1, 0x35, 0x95, 0x5a, 0x00, 0xbd, 0xa4, 0x3c, 0x2d, 0xa6, 0x23, 0xfb, 0x72, 0xda, 0xa8, 0x7b, 0xf8, 0x33, 0x13, 0xd9, 0xfd, 0x2a, 0x07, 0xfd, 0xc3, 0x9c, 0x54, 0xac, 0xbc, 0x0f, 0x3f, 0x93, 0xa0, 0x6c, 0xd0, 0x10, 0xde, 0x0c, 0x1b, 0x2a, 0x59, 0x1e, 0x55, 0x44, 0xce, 0x19, 0x0f, 0x4f, 0xc7, 0xf5, 0x5d, 0x34, 0x23, 0xe9, 0xfb, 0xb9, 0x14, 0x0b, 0x9e, 0x85, 0xcf, 0x26, 0x44, 0x7f, 0xa2, 0x54, 0x94, 0x42, 0x86, 0xcf, 0xf2, 0x3c, 0x5f, 0x17, 0x93, 0x95, 0x1b, 0x8d, 0xc7, 0x79, 0x7e, 0x76, 0x16, 0x29, 0x7a, 0xa7, 0x86, 0xa4, 0x64, 0x73, 0x1e, 0xa6, 0x60, 0x3f, 0x95, 0xeb, 0x40, 0xbb, 0x42, 0x18, 0xa7, 0x72, 0x55, 0x91, 0x3b, 0xeb, 0x42, 0x78, 0x36, 0xd6, 0xba, 0xdd, 0x3e, 0x63, 0x44, 0x16, 0x4a, 0xc0, 0x42, 0x22, 0xb3, 0x55, 0x77, 0xbf, 0x53, 0xa2, 0x3f, 0x51, 0x4d, 0xb2, 0x8c, 0xf1, 0xb9, 0xb5, 0xc7, 0xc9, 0x4c, 0x9e, 0xd7, 0x77, 0x68, 0x1c, 0xcd, 0x84, 0xcc, 0xa8, 0x1c, 0x4a, 0x92, 0xb1, 0x45, 0x13, 0x4e, 0x8c, 0xc1, 0xe2, 0x6e, 0xd8, 0x14, 0x24, 0x13, 0x4b, 0x50, 0xfc, 0x29, 0xac, 0x7a, 0x01, 0xff, 0xe5, 0x7c, 0x46, 0xbc, 0xf1, 0xc0, 0x7c, 0x82, 0x4f, 0xfc, 0x75, 0xd0, 0x28, 0xa2, 0x16, 0x8d, 0x85, 0xa1, 0x61, 0x5f, 0xd3, 0xf0, 0x14, 0x96, 0x46, 0x66, 0xb8, 0xa4, 0x6c, 0x5e, 0xa8, 0x70, 0x26, 0xca, 0x2c, 0xea, 0x39, 0xb7, 0x0e, 0x4a, 0x32, 0xa3, 0x65, 0xeb, 0xf1, 0x19, 0xb8, 0x6b, 0xad, 0x19, 0xce, 0x84, 0x52, 0xa2, 0x0a, 0xc1, 0xa6, 0xf5, 0x6c, 0x01, 0x8f, 0xbc, 0xe7, 0x86, 0x83, 0x66, 0xa3, 0xab, 0x35, 0x3b, 0xe4, 0x82, 0xd3, 0x8d, 0x77, 0x93, 0x53, 0xb0, 0x73, 0x6b, 0x85, 0x31, 0x6a, 0xf2, 0xc2, 0x38, 0xd4, 0xf5, 0x11, 0xf6, 0x88, 0xd2, 0x85, 0x6c, 0x40, 0x55, 0x2d, 0x98, 0x46, 0xb8, 0x85, 0x64, 0xbb, 0x7b, 0x58, 0x88, 0x5b, 0x80, 0xbb, 0x6f, 0x43, 0x9a, 0xbe, 0x20, 0xed, 0x7b, 0x92, 0x2a, 0x76, 0x4b, 0x57, 0x4a, 0xc2, 0x99, 0xe7, 0x42, 0x56, 0xa1, 0x61, 0x92, 0x37, 0x0e, 0x7e, 0xfd, 0x1c, 0xb0, 0xb9, 0x25, 0xe5, 0x82, 0x76, 0xa0, 0xf9, 0xe4, 0xf4, 0x10, 0x34, 0xed, 0x41, 0x8c, 0xf5, 0x41, 0xac, 0x83, 0x05, 0x70, 0xb2, 0x23, 0x33, 0x39, 0xd3, 0x76, 0x6e, 0x80, 0x5a, 0x4f, 0x47, 0x96, 0x7c, 0xd3, 0x91, 0x0d, 0x05, 0x4d, 0xc2, 0x64, 0x9a, 0xb1, 0x5b, 0x94, 0x96, 0xa4, 0x69, 0x62, 0xbc, 0x61, 0x09, 0x70, 0xb9, 0x98, 0x24, 0x3f, 0x7d, 0xff, 0xb7, 0x7f, 0xfc, 0xe7, 0xdf, 0xdf, 0xa2, 0x23, 0x84, 0x86, 0x15, 0x3d, 0x61, 0x60, 0x0e, 0xee, 0xcd, 0x98, 0x93, 0xc2, 0xa0, 0xe6, 0x2f, 0xdf, 0xa0, 0x97, 0x44, 0x01, 0x4c, 0xf7, 0xe8, 0x2b, 0x51, 0x2a, 0x32, 0xa7, 0xd3, 0x11, 0xac, 0xeb, 0x2d, 0x36, 0x0e, 0x63, 0xc4, 0x32, 0x78, 0xb4, 0x6b, 0x70, 0x32, 0x1c, 0x06, 0x43, 0x34, 0x6d, 0x6a, 0xc2, 0xdb, 0x55, 0xda, 0x43, 0x9c, 0x7c, 0x05, 0x9e, 0xc0, 0x64, 0xe2, 0xb4, 0x58, 0x40, 0x91, 0xe0, 0x69, 0xc9, 0xd2, 0xf7, 0x31, 0x96, 0x14, 0x62, 0xdb, 0x6d, 0xe8, 0xf9, 0x38, 0xf9, 0x52, 0x8f, 0x5b, 0x0b, 0xa6, 0x23, 0xbb, 0x3c, 0xd9, 0x37, 0xe1, 0xa8, 0x07, 0x3f, 0xfe, 0xf5, 0x07, 0xf4, 0x46, 0x28, 0x21, 0xd1, 0x97, 0x57, 0x6f, 0x1e, 0x34, 0x5d, 0xd6, 0x95, 0x36, 0x7b, 0x78, 0xd0, 0x6c, 0x23, 0xfc, 0x90, 0xe1, 0x10, 0x10, 0x52, 0x99, 0x8d, 0xb4, 0xd9, 0xd7, 0x7a, 0x64, 0xf7, 0xdd, 0x1a, 0xbd, 0x2f, 0x22, 0xea, 0x8e, 0x84, 0xa8, 0x77, 0x05, 0x7e, 0x86, 0x97, 0xdf, 0xfe, 0x53, 0x9f, 0xf6, 0xf5, 0x7d, 0xa3, 0x68, 0x85, 0xde, 0xd5, 0x8a, 0x55, 0x07, 0xce, 0xc9, 0x06, 0xad, 0xf5, 0x76, 0x61, 0xd6, 0xe0, 0x64, 0xdc, 0xb8, 0x75, 0x8f, 0xdf, 0xec, 0xa7, 0xef, 0xbf, 0xfb, 0x17, 0xfa, 0x3d, 0xbb, 0x60, 0xe8, 0x5c, 0xf0, 0x9c, 0xcd, 0x17, 0x92, 0x28, 0x26, 0xf8, 0x31, 0x60, 0x52, 0xc2, 0xbf, 0xa0, 0x6a, 0x29, 0xe4, 0xfb, 0xc6, 0x38, 0x0a, 0x63, 0xd4, 0x4e, 0x1c, 0x07, 0x27, 0x2d, 0x29, 0x91, 0x7a, 0x13, 0x90, 0x41, 0x86, 0xfb, 0x31, 0xee, 0x06, 0x64, 0x9e, 0x7f, 0x0a, 0xff, 0xda, 0x04, 0x52, 0xd2, 0x5c, 0xe9, 0x00, 0xc6, 0xc9, 0xb9, 0x96, 0x43, 0xd7, 0xe4, 0x96, 0x66, 0xc6, 0xc6, 0xce, 0x0e, 0x12, 0x32, 0x38, 0x2d, 0x69, 0xaa, 0x0c, 0x00, 0x4b, 0x96, 0xb3, 0xcf, 0x59, 0xa3, 0x36, 0xda, 0x6d, 0x66, 0x9d, 0x8c, 0xc7, 0xbf, 0xea, 0x64, 0x04, 0xc8, 0x91, 0x6d, 0x82, 0x39, 0xfb, 0x50, 0x36, 0x6f, 0xd3, 0xd2, 0x04, 0xc4, 0x1a, 0x51, 0xb2, 0x0c, 0x3d, 0xd3, 0x26, 0xee, 0xe5, 0x1f, 0x00, 0x55, 0xd4, 0x1a, 0x31, 0x64, 0xd8, 0x17, 0x63, 0x00, 0xc5, 0xda, 0xe5, 0x60, 0x09, 0x82, 0x60, 0x3a, 0xb2, 0x4b, 0xe0, 0x5c, 0xac, 0xcd, 0xd6, 0x7e, 0xc6, 0xeb, 0x85, 0x42, 0xea, 0xbe, 0x06, 0x29, 0x5d, 0x2a, 0xec, 0x59, 0x36, 0x0d, 0xcb, 0x30, 0xaa, 0x4b, 0x92, 0xd2, 0x02, 0x52, 0x0b, 0x95, 0x31, 0xbe, 0xbe, 0xbe, 0x7c, 0xf5, 0x0b, 0x78, 0x36, 0xda, 0x37, 0xb3, 0x06, 0xea, 0x80, 0x53, 0x99, 0x35, 0x75, 0x3b, 0xea, 0x99, 0x7b, 0xb5, 0x99, 0xfe, 0xa5, 0x4c, 0xde, 0xe3, 0x9f, 0xe0, 0x1c, 0x60, 0x77, 0x0c, 0x4c, 0xce, 0xed, 0x10, 0x29, 0xb1, 0xc3, 0x2a, 0x1d, 0x1e, 0x2d, 0x9d, 0xae, 0x5d, 0x7c, 0x39, 0x1f, 0x1c, 0x37, 0x21, 0xb0, 0x6d, 0x65, 0xed, 0x15, 0x43, 0x9c, 0x7c, 0x4c, 0xdc, 0xfd, 0x80, 0xde, 0xde, 0x7c, 0x06, 0xf1, 0x9d, 0x11, 0xd5, 0xc6, 0xb7, 0xae, 0x3f, 0xc6, 0x02, 0xa1, 0xc8, 0x05, 0x3c, 0x63, 0x04, 0x29, 0xde, 0x42, 0x5f, 0x2d, 0x4a, 0xc5, 0x6a, 0xc8, 0x44, 0x23, 0xbd, 0x68, 0x08, 0x42, 0x04, 0xf7, 0x0f, 0x27, 0x67, 0xa5, 0xcb, 0x7e, 0x39, 0x93, 0xd5, 0x92, 0x48, 0x18, 0x91, 0x34, 0xa5, 0x35, 0x74, 0x4e, 0xc1, 0x8c, 0xf1, 0x1d, 0x5f, 0x5c, 0xb5, 0xda, 0xc1, 0xcc, 0xaa, 0xb2, 0x03, 0xbc, 0x45, 0x70, 0x51, 0x97, 0x82, 0x64, 0x17, 0x4e, 0xaf, 0x06, 0xf1, 0x9d, 0x99, 0x41, 0xed, 0x54, 0x27, 0xd1, 0x69, 0xfb, 0xb6, 0x58, 0x82, 0x27, 0xff, 0x03, 0x94, 0xf6, 0xbb, 0x49, 0x25, 0xab, 0x55, 0x52, 0x42, 0xf5, 0x30, 0xa9, 0xf9, 0x06, 0xb2, 0x5d, 0xfc, 0x0a, 0x60, 0x0b, 0xb8, 0x58, 0x7a, 0x7e, 0x94, 0x2f, 0x00, 0x25, 0x1d, 0x83, 0x0b, 0x03, 0x26, 0xbc, 0x21, 0x9e, 0xbf, 0xca, 0xa9, 0x4a, 0x0b, 0x0f, 0x8f, 0x48, 0xcd, 0x46, 0x2e, 0x5b, 0xfa, 0x81, 0x2a, 0x28, 0xf7, 0x64, 0x9c, 0xc8, 0xe0, 0xcf, 0x8d, 0xe0, 0x9e, 0xef, 0x66, 0x34, 0x9c, 0x71, 0xb2, 0xca, 0x44, 0xba, 0xa8, 0xa0, 0x55, 0x0b, 0xe6, 0x54, 0xbd, 0x2e, 0xa9, 0x7e, 0x7c, 0x79, 0x7f, 0x99, 0x79, 0x9b, 0x52, 0xe8, 0x07, 0x0c, 0xb8, 0x23, 0x7f, 0x7b, 0xf3, 0xe6, 0xf3, 0x58, 0xcb, 0x04, 0x33, 0x5b, 0xcd, 0x02, 0x25, 0x2e, 0xd8, 0x1d, 0xcd, 0xbc, 0x89, 0x7f, 0x82, 0x7b, 0x35, 0xe7, 0xa9, 0xae, 0x39, 0x4f, 0x37, 0xa5, 0x12, 0x47, 0x47, 0xf7, 0xd0, 0x35, 0x6b, 0x4f, 0x3f, 0x4c, 0x1e, 0x56, 0xb8, 0x2d, 0x62, 0x38, 0xd2, 0xc0, 0xd8, 0x1a, 0x10, 0xbf, 0x21, 0xaa, 0x08, 0xf2, 0x52, 0x40, 0x21, 0xf2, 0xb6, 0x08, 0x0d, 0x37, 0xb0, 0xf9, 0x23, 0x88, 0xc3, 0xb1, 0x7f, 0xdc, 0x0a, 0x57, 0x4b, 0x00, 0x17, 0x48, 0x49, 0xe7, 0xae, 0xf3, 0xb6, 0x93, 0x27, 0xb8, 0xc1, 0xd1, 0xda, 0x8f, 0xd6, 0x3b, 0x78, 0xeb, 0x20, 0xb2, 0x67, 0xbc, 0x83, 0xba, 0x8e, 0xa3, 0xc7, 0x42, 0x0f, 0x31, 0xda, 0x98, 0xe3, 0x85, 0xc5, 0xf1, 0x51, 0xeb, 0x3a, 0x91, 0xe9, 0x47, 0x2c, 0x37, 0xc2, 0x81, 0x0b, 0x6f, 0x9a, 0xf9, 0x2b, 0x2b, 0xdf, 0x01, 0x11, 0xff, 0xf8, 0xf7, 0xef, 0xd0, 0x79, 0xbb, 0x40, 0x87, 0x3c, 0x74, 0xfe, 0x52, 0xf0, 0x79, 0x82, 0x4f, 0x8c, 0xb0, 0xce, 0xb7, 0x27, 0x58, 0xb7, 0x64, 0x66, 0x56, 0xc7, 0xc2, 0xe5, 0x55, 0xe8, 0x5e, 0xb2, 0x3a, 0x72, 0x1a, 0x0d, 0x77, 0x03, 0x43, 0xd5, 0x18, 0xb7, 0x5c, 0x8d, 0xd6, 0xb4, 0x6c, 0x68, 0xbb, 0x69, 0x17, 0x30, 0x23, 0x5d, 0xd1, 0xa6, 0x01, 0xd2, 0x1c, 0x56, 0xa1, 0x15, 0x8c, 0xc7, 0xa0, 0xa2, 0x07, 0x68, 0xbf, 0x65, 0xea, 0x81, 0xe9, 0x98, 0x36, 0x32, 0x4b, 0xf0, 0x60, 0x05, 0x57, 0xa5, 0x42, 0x64, 0x21, 0xbe, 0x7a, 0x7b, 0x7d, 0x83, 0x7b, 0x4a, 0xba, 0xed, 0x4b, 0x4f, 0x45, 0xa5, 0xe7, 0x46, 0xe6, 0xf5, 0x07, 0x14, 0x6c, 0x9a, 0x99, 0x83, 0xf2, 0xa2, 0x7e, 0x58, 0xbc, 0xd7, 0x25, 0x7c, 0xcc, 0xc9, 0x5a, 0x91, 0x12, 0x2a, 0xfa, 0xc3, 0x02, 0xa6, 0xe6, 0xfb, 0xd1, 0x81, 0x03, 0xc0, 0xba, 0x35, 0xe1, 0x50, 0x5d, 0xa0, 0xfe, 0xe2, 0x68, 0x9f, 0x94, 0xf0, 0xf6, 0x38, 0x25, 0xb9, 0x33, 0x1e, 0x68, 0xa9, 0x6d, 0xe8, 0xd2, 0xa9, 0x5f, 0xed, 0x9f, 0x3e, 0x7d, 0xa0, 0xda, 0xe3, 0xa8, 0xd5, 0x13, 0x40, 0x4a, 0x7c, 0x4d, 0xc0, 0x02, 0xbe, 0x21, 0x3a, 0xac, 0xd9, 0xba, 0x96, 0x4a, 0x0a, 0x61, 0xe4, 0xbc, 0xf3, 0xb0, 0x95, 0x07, 0xbf, 0xe0, 0xc1, 0x5e, 0x40, 0x62, 0x6e, 0x78, 0x6a, 0x26, 0xba, 0x5e, 0x72, 0x47, 0x5f, 0xe4, 0xe1, 0x13, 0x1e, 0x48, 0x18, 0xc0, 0x73, 0xf6, 0xb2, 0xf2, 0x21, 0x27, 0x68, 0xc3, 0x49, 0x5d, 0x53, 0x9e, 0x9d, 0x17, 0xac, 0xcc, 0x3c, 0x90, 0xf5, 0x75, 0xf4, 0x1e, 0x02, 0xeb, 0x42, 0x17, 0x5e, 0x04, 0x3a, 0x5a, 0x83, 0x4b, 0xca, 0xe7, 0xaa, 0x00, 0x65, 0xed, 0x8c, 0x0e, 0x7c, 0xb8, 0x94, 0x6a, 0x18, 0x29, 0x38, 0x71, 0x0c, 0x71, 0x94, 0x13, 0xa8, 0x42, 0xc0, 0x8a, 0x13, 0x6a, 0x32, 0xc5, 0x23, 0x4e, 0x2f, 0x80, 0x2e, 0xe0, 0xf5, 0x2d, 0xbc, 0xd0, 0x43, 0x0a, 0x40, 0x7b, 0x38, 0x2d, 0x08, 0x87, 0x64, 0x3b, 0x68, 0x19, 0x05, 0x24, 0x3a, 0xaa, 0xc8, 0xf4, 0x4b, 0xbe, 0x83, 0x49, 0x15, 0xac, 0xb1, 0x8f, 0x7a, 0xf7, 0x0d, 0x21, 0x7b, 0xf5, 0xbf, 0xe5, 0x23, 0xc8, 0xc5, 0x8f, 0xd2, 0xea, 0xd8, 0xd8, 0x76, 0x3b, 0xc7, 0x85, 0x36, 0xfd, 0x50, 0x5f, 0xf0, 0xe7, 0xe6, 0xb4, 0x27, 0x7a, 0x73, 0x7f, 0x05, 0x97, 0x50, 0x09, 0x64, 0xb8, 0x82, 0x1e, 0xb8, 0xa1, 0xc8, 0xfc, 0x7e, 0x80, 0x4c, 0x27, 0xe8, 0x47, 0x92, 0xaa, 0x85, 0xe4, 0xd1, 0xfa, 0xd0, 0x29, 0xb8, 0x5c, 0x77, 0x94, 0xf9, 0x0e, 0x8b, 0xdd, 0x00, 0x1e, 0xe8, 0xab, 0x28, 0x95, 0x4d, 0xb8, 0xc2, 0x4e, 0xd5, 0xf0, 0x06, 0x5a, 0x01, 0x1c, 0x62, 0xe0, 0x10, 0xf4, 0x00, 0xe6, 0x56, 0x30, 0xd2, 0x51, 0x82, 0xd7, 0x03, 0x7d, 0x61, 0x0d, 0x7f, 0x77, 0xfd, 0xf6, 0x0b, 0xc8, 0x69, 0x12, 0x76, 0x62, 0xf9, 0xbd, 0xb7, 0xd2, 0x56, 0x87, 0xfa, 0x6b, 0xd0, 0xe2, 0x10, 0xb6, 0x0f, 0x6b, 0x7f, 0xdd, 0x8d, 0x35, 0x6d, 0xef, 0x26, 0xd6, 0xaa, 0x66, 0x7e, 0x98, 0x4f, 0xf0, 0x22, 0xda, 0xaf, 0x2f, 0x1f, 0xa6, 0x61, 0x0b, 0x80, 0xd8, 0x23, 0xe3, 0x96, 0x0f, 0xdb, 0xfb, 0xc8, 0x47, 0xd6, 0x9d, 0x27, 0xa9, 0xbe, 0x2d, 0xc9, 0xca, 0xc3, 0xf6, 0x8e, 0xd2, 0x6c, 0xee, 0x28, 0x08, 0x82, 0x39, 0x03, 0x39, 0x46, 0xca, 0xe6, 0x37, 0xd8, 0xf7, 0xdd, 0x59, 0x1d, 0xb4, 0x54, 0x8b, 0x1e, 0x3f, 0x28, 0xfd, 0x76, 0x3f, 0xcf, 0x7e, 0x14, 0x8e, 0x1f, 0x06, 0xcd, 0x78, 0x71, 0x0c, 0xaf, 0xdd, 0xee, 0xcf, 0x81, 0xa6, 0x1b, 0xce, 0xe3, 0x90, 0x6d, 0x9a, 0x50, 0x3f, 0xd0, 0x0b, 0x9b, 0x3f, 0x8e, 0xff, 0x64, 0x90, 0xd3, 0x83, 0x5d, 0x76, 0xbb, 0xab, 0x1c, 0x41, 0xad, 0x90, 0xd1, 0xdd, 0x61, 0xfa, 0xe3, 0x4e, 0x69, 0xdb, 0x6b, 0x1e, 0x4e, 0x73, 0xb6, 0x67, 0x6d, 0x11, 0x77, 0x4e, 0x40, 0xb7, 0xaa, 0x1b, 0xc6, 0x98, 0xd3, 0x25, 0xba, 0x70, 0x03, 0xdd, 0x53, 0xba, 0x47, 0x97, 0x43, 0x3b, 0xfe, 0x0c, 0x8c, 0x07, 0xbd, 0x03, 0x83, 0x8d, 0x47, 0x96, 0xab, 0x7b, 0x71, 0x65, 0x62, 0xa5, 0x55, 0xf6, 0xff, 0x3a, 0x3e, 0xd7, 0x8c, 0xef, 0x9c, 0x1f, 0xf4, 0x09, 0x97, 0x3a, 0x6d, 0x40, 0x32, 0xf2, 0xb6, 0x9d, 0xf1, 0xe0, 0xb9, 0xee, 0xff, 0xf6, 0xdf, 0x6d, 0xa3, 0x6c, 0x60, 0x5b, 0xc4, 0x6e, 0x33, 0x7d, 0x28, 0x12, 0xa1, 0x67, 0xb2, 0x7d, 0x39, 0xf4, 0xfe, 0xe6, 0x17, 0xac, 0x91, 0xf9, 0x7d, 0xf7, 0xbf, 0x31, 0xf1, 0x0b, 0x6f, 0xf5, 0x15, 0x00, 0x00
};
static const size_t html_page_gz_len = 1928;

// Simulated sensor data
static float battery_voltage = 12.6;
static int motor_rpm = 0;
static int motor_speed_percent = 0;  // 0-100%

// Configure ESC protocol and recalculate PWM parameters
static void configure_protocol(esc_protocol_t protocol) {
    current_protocol = protocol;
    
    switch (protocol) {
        case PROTOCOL_STANDARD:  // Standard PWM: 50Hz, 1-2ms pulses
            pwm_frequency = 50;
            pwm_resolution = LEDC_TIMER_14_BIT;  // 16384 steps
            pwm_min_duty = 820;   // 1ms: 1/20 * 16384 = 820
            pwm_max_duty = 1638;  // 2ms: 2/20 * 16384 = 1638
            ESP_LOGI(TAG, "Protocol: Standard PWM (50Hz, 1-2ms)");
            break;
            
        case PROTOCOL_ONESHOT125:  // OneShot125: 3-8kHz, 125-250µs pulses
            pwm_frequency = 4000;  // 4kHz update rate
            pwm_resolution = LEDC_TIMER_13_BIT;  // 8192 steps
            pwm_min_duty = 410;    // 125µs: 125µs / 250µs * 8192 = 4096, scaled to 13-bit
            pwm_max_duty = 819;    // 250µs: 250µs / 250µs * 8192 = 8192, scaled to 13-bit
            ESP_LOGI(TAG, "Protocol: OneShot125 (4kHz, 125-250µs)");
            break;
            
        case PROTOCOL_ONESHOT42:  // OneShot42: 3-8kHz, 42-84µs pulses
            pwm_frequency = 8000;  // 8kHz update rate
            pwm_resolution = LEDC_TIMER_13_BIT;  // 8192 steps
            pwm_min_duty = 275;    // 42µs
            pwm_max_duty = 549;    // 84µs
            ESP_LOGI(TAG, "Protocol: OneShot42 (8kHz, 42-84µs)");
            break;
            
        case PROTOCOL_MULTISHOT:  // Multishot: 32kHz, 5-25µs pulses
            pwm_frequency = 32000;  // 32kHz update rate
            pwm_resolution = LEDC_TIMER_12_BIT;  // 4096 steps
            pwm_min_duty = 205;    // 5µs
            pwm_max_duty = 1024;   // 25µs
            ESP_LOGI(TAG, "Protocol: Multishot (32kHz, 5-25µs)");
            break;
    }
    
    // Reconfigure LEDC timer with new settings
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = pwm_resolution,
        .timer_num        = MOTOR_PWM_TIMER,
        .freq_hz          = pwm_frequency,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
}

// Convert speed percentage (0-100%) to PWM duty cycle based on current protocol
static uint32_t speed_to_pwm(int speed_percent) {
    if (speed_percent < 0) speed_percent = 0;
    if (speed_percent > 100) speed_percent = 100;
    // Map 0-100% to protocol-specific min-max duty range
    return pwm_min_duty + ((speed_percent * (pwm_max_duty - pwm_min_duty)) / 100);
}

// WiFi connection status tracking
static bool wifi_connected = false;
static char wifi_connected_ssid[33] = "";
static char wifi_ip_address[16] = "";
static char wifi_status_message[64] = "Not connected";
static int wifi_retry_count = 0;
static const int MAX_WIFI_RETRIES = 5;

// HTTP GET handler for main page
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_send(req, (const char *)html_page_gz, html_page_gz_len);
    return ESP_OK;
}

// HTTP GET handler for status API
static esp_err_t status_handler(httpd_req_t *req)
{
    char json[200];
    snprintf(json, sizeof(json), 
        "{\"battery\":%.1f,\"rpm\":%d}",
        battery_voltage, motor_rpm);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

// HTTP GET handler for WiFi status API
static esp_err_t wifi_status_handler(httpd_req_t *req)
{
    char json[256];
    snprintf(json, sizeof(json), 
        "{\"connected\":%s,\"ssid\":\"%s\",\"ip\":\"%s\",\"message\":\"%s\"}",
        wifi_connected ? "true" : "false",
        wifi_connected_ssid,
        wifi_ip_address,
        wifi_status_message);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

// HTTP POST handler for battery reset
static esp_err_t battery_reset_handler(httpd_req_t *req)
{
    battery_voltage = 12.6 + (rand() % 10) * 0.1;
    ESP_LOGI(TAG, "Battery reset! Voltage: %.1fV", battery_voltage);
    
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// HTTP POST handler for motor start (default 50% speed)
static esp_err_t motor_start_handler(httpd_req_t *req)
{
    motor_speed_percent = 50;
    motor_rpm = (motor_speed_percent * 4000) / 100;
    
    uint32_t duty = speed_to_pwm(motor_speed_percent);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWM_CHANNEL);
    
    ESP_LOGI(TAG, "Motor started: %d%% (%d RPM, PWM: %lu)", motor_speed_percent, motor_rpm, duty);
    
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// HTTP POST handler for motor stop
static esp_err_t motor_stop_handler(httpd_req_t *req)
{
    motor_speed_percent = 0;
    motor_rpm = 0;
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWM_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWM_CHANNEL);
    
    ESP_LOGI(TAG, "Motor stopped");
    
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// HTTP POST handler for motor speed control (JSON: {"speed": 0-100})
static esp_err_t motor_speed_handler(httpd_req_t *req)
{
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse JSON: {"speed":75}
    char *speed_str = strstr(buf, "\"speed\":");
    if (speed_str) {
        speed_str += 8;  // Skip '"speed":'
        motor_speed_percent = atoi(speed_str);
        
        if (motor_speed_percent < 0) motor_speed_percent = 0;
        if (motor_speed_percent > 100) motor_speed_percent = 100;
        
        motor_rpm = (motor_speed_percent * 4000) / 100;
        
        uint32_t duty = speed_to_pwm(motor_speed_percent);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWM_CHANNEL, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWM_CHANNEL);
        
        ESP_LOGI(TAG, "Motor speed set: %d%% (%d RPM, PWM: %lu)", motor_speed_percent, motor_rpm, duty);
        
        httpd_resp_send(req, "OK", 2);
        return ESP_OK;
    }
    
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    return ESP_OK;
}

// HTTP POST handler for protocol change (JSON: {"protocol": "standard"|"oneshot125"|"oneshot42"|"multishot"})
static esp_err_t motor_protocol_handler(httpd_req_t *req)
{
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse protocol from JSON
    esc_protocol_t new_protocol = PROTOCOL_STANDARD;
    if (strstr(buf, "oneshot125")) {
        new_protocol = PROTOCOL_ONESHOT125;
    } else if (strstr(buf, "oneshot42")) {
        new_protocol = PROTOCOL_ONESHOT42;
    } else if (strstr(buf, "multishot")) {
        new_protocol = PROTOCOL_MULTISHOT;
    } else if (strstr(buf, "standard")) {
        new_protocol = PROTOCOL_STANDARD;
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown protocol");
        return ESP_FAIL;
    }
    
    configure_protocol(new_protocol);
    
    // Reset motor to off when changing protocol
    motor_speed_percent = 0;
    motor_rpm = 0;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWM_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWM_CHANNEL);
    
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// HTTP POST handler to clear WiFi credentials
static esp_err_t wifi_clear_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Clearing WiFi credentials from NVS");
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_key(nvs_handle, "ssid");
        nvs_erase_key(nvs_handle, "password");
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        
        // Switch back to AP-only mode
        esp_wifi_set_mode(WIFI_MODE_AP);
        
        ESP_LOGI(TAG, "WiFi credentials cleared, switched to AP mode");
        httpd_resp_sendstr(req, "WiFi credentials cleared! Device in AP-only mode.");
    } else {
        ESP_LOGE(TAG, "Failed to open NVS");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to clear credentials");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// HTTP POST handler for OTA update
static esp_err_t ota_update_handler(httpd_req_t *req)
{
    char buf[1024];
    esp_ota_handle_t ota_handle;
    const esp_partition_t *update_partition = NULL;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    
    ESP_LOGI(TAG, "Starting OTA update...");
    
    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08lx, but running from offset 0x%08lx",
                 configured->address, running->address);
    }
    
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx",
             update_partition->subtype, update_partition->address);
    
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }
    
    int remaining = req->content_len;
    int received = 0;
    
    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, sizeof(buf) < remaining ? sizeof(buf) : remaining);
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive firmware");
            return ESP_FAIL;
        }
        
        err = esp_ota_write(ota_handle, buf, recv_len);
        if (err != ESP_OK) {
            esp_ota_abort(ota_handle);
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            return ESP_FAIL;
        }
        
        received += recv_len;
        remaining -= recv_len;
        
        if (received % 102400 == 0) {
            ESP_LOGI(TAG, "Written %d / %d bytes", received, req->content_len);
        }
    }
    
    ESP_LOGI(TAG, "Total written: %d bytes", received);
    
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_FAIL;
    }
    
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "OTA update successful! Rebooting in 3 seconds...");
    httpd_resp_sendstr(req, "Update successful! Device rebooting...");
    
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    esp_restart();
    
    return ESP_OK;
}

// HTTP GET handler for WiFi scan
static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Starting WiFi scan...");
    
    // Disconnect STA if it's trying to connect (this blocks scanning)
    esp_wifi_disconnect();
    vTaskDelay(200 / portTICK_PERIOD_MS);
    
    // Clear any previous scan results
    esp_wifi_scan_stop();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300
            }
        }
    };
    
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed to start: %s", esp_err_to_name(err));
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    ESP_LOGI(TAG, "Scan complete, found %d networks", ap_count);
    
    if (ap_count == 0) {
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    
    wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_records == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    
    // Build JSON response
    char *json = (char *)malloc(4096);
    if (json == NULL) {
        free(ap_records);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    int offset = sprintf(json, "[");
    for (int i = 0; i < ap_count && i < 20; i++) {
        offset += sprintf(json + offset, "%s{\"ssid\":\"%s\",\"rssi\":%d}",
                         i > 0 ? "," : "",
                         ap_records[i].ssid,
                         ap_records[i].rssi);
    }
    offset += sprintf(json + offset, "]");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, offset);
    
    free(json);
    free(ap_records);
    
    ESP_LOGI(TAG, "WiFi scan complete: %d networks found", ap_count);
    return ESP_OK;
}

// Event handler for WiFi station events
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Station started, connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        wifi_ip_address[0] = '\0';
        wifi_event_sta_disconnected_t* disconn = (wifi_event_sta_disconnected_t*) event_data;
        const char* reason_str = "Unknown";
        
        // Decode disconnection reason
        switch(disconn->reason) {
            case WIFI_REASON_UNSPECIFIED: reason_str = "Unspecified"; break;
            case WIFI_REASON_AUTH_EXPIRE: reason_str = "Auth expired"; break;
            case WIFI_REASON_AUTH_LEAVE: reason_str = "Auth leave"; break;
            case WIFI_REASON_ASSOC_EXPIRE: reason_str = "Assoc expired"; break;
            case WIFI_REASON_ASSOC_TOOMANY: reason_str = "Too many assoc"; break;
            case WIFI_REASON_NOT_AUTHED: reason_str = "Not authenticated"; break;
            case WIFI_REASON_NOT_ASSOCED: reason_str = "Not associated"; break;
            case WIFI_REASON_ASSOC_LEAVE: reason_str = "Assoc leave"; break;
            case WIFI_REASON_ASSOC_NOT_AUTHED: reason_str = "Assoc not authed"; break;
            case WIFI_REASON_DISASSOC_PWRCAP_BAD: reason_str = "Bad power cap"; break;
            case WIFI_REASON_DISASSOC_SUPCHAN_BAD: reason_str = "Bad sup channel"; break;
            case WIFI_REASON_BSS_TRANSITION_DISASSOC: reason_str = "BSS transition"; break;
            case WIFI_REASON_IE_INVALID: reason_str = "Invalid IE"; break;
            case WIFI_REASON_MIC_FAILURE: reason_str = "MIC failure"; break;
            case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: reason_str = "4-way handshake timeout"; break;
            case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: reason_str = "Group key timeout"; break;
            case WIFI_REASON_IE_IN_4WAY_DIFFERS: reason_str = "IE differs in 4-way"; break;
            case WIFI_REASON_GROUP_CIPHER_INVALID: reason_str = "Invalid group cipher"; break;
            case WIFI_REASON_PAIRWISE_CIPHER_INVALID: reason_str = "Invalid pairwise cipher"; break;
            case WIFI_REASON_AKMP_INVALID: reason_str = "Invalid AKMP"; break;
            case WIFI_REASON_UNSUPP_RSN_IE_VERSION: reason_str = "Unsupported RSN IE version"; break;
            case WIFI_REASON_INVALID_RSN_IE_CAP: reason_str = "Invalid RSN IE cap"; break;
            case WIFI_REASON_802_1X_AUTH_FAILED: reason_str = "802.1X auth failed"; break;
            case WIFI_REASON_CIPHER_SUITE_REJECTED: reason_str = "Cipher suite rejected"; break;
            case WIFI_REASON_INVALID_PMKID: reason_str = "Invalid PMKID"; break;
            case WIFI_REASON_BEACON_TIMEOUT: reason_str = "Beacon timeout"; break;
            case WIFI_REASON_NO_AP_FOUND: reason_str = "No AP found"; break;
            case WIFI_REASON_AUTH_FAIL: reason_str = "Authentication failed (wrong password?)"; break;
            case WIFI_REASON_ASSOC_FAIL: reason_str = "Association failed"; break;
            case WIFI_REASON_HANDSHAKE_TIMEOUT: reason_str = "Handshake timeout"; break;
            case WIFI_REASON_CONNECTION_FAIL: reason_str = "Connection failed"; break;
            case WIFI_REASON_AP_TSF_RESET: reason_str = "AP TSF reset"; break;
            case WIFI_REASON_ROAMING: reason_str = "Roaming"; break;
        }
        
        // Show SSID that failed
        char failed_ssid[33] = "";
        if (disconn->ssid_len > 0) {
            snprintf(failed_ssid, sizeof(failed_ssid), "%.*s", disconn->ssid_len, disconn->ssid);
            ESP_LOGW(TAG, "Disconnected from '%s' (reason: %d - %s)", failed_ssid, disconn->reason, reason_str);
        } else {
            ESP_LOGW(TAG, "Disconnected from AP (reason: %d - %s)", disconn->reason, reason_str);
        }
        
        // Limit retry attempts to prevent infinite reconnection loops
        wifi_retry_count++;
        if (wifi_retry_count < MAX_WIFI_RETRIES) {
            ESP_LOGI(TAG, "Retry %d/%d - Attempting to reconnect...", wifi_retry_count, MAX_WIFI_RETRIES);
            snprintf(wifi_status_message, sizeof(wifi_status_message), 
                     "Retry %d/%d: %s", wifi_retry_count, MAX_WIFI_RETRIES, reason_str);
            esp_wifi_connect();
        } else {
            ESP_LOGW(TAG, "Max retries reached for '%s'. Giving up. AP Mode still active at 192.168.4.1", failed_ssid);
            snprintf(wifi_status_message, sizeof(wifi_status_message), 
                     "Failed after %d retries. Use web UI to try another network.", MAX_WIFI_RETRIES);
            // Don't retry anymore - AP mode remains functional
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        wifi_connected = true;
        wifi_retry_count = 0; // Reset retry counter on successful connection
        snprintf(wifi_ip_address, sizeof(wifi_ip_address), IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(wifi_status_message, sizeof(wifi_status_message), 
                 "✓ Connected! IP: %s", wifi_ip_address);
        ESP_LOGI(TAG, "✓ Connected! Got IP: %s", wifi_ip_address);
        
        // Get the connected SSID
        wifi_config_t wifi_config;
        if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) == ESP_OK) {
            strncpy(wifi_connected_ssid, (char*)wifi_config.sta.ssid, sizeof(wifi_connected_ssid) - 1);
            wifi_connected_ssid[sizeof(wifi_connected_ssid) - 1] = '\0';
        }
    }
}

// HTTP POST handler for WiFi connect
static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse JSON (simple manual parsing)
    char ssid[33] = {0};
    char password[64] = {0};
    
    char *ssid_start = strstr(buf, "\"ssid\":\"");
    char *pass_start = strstr(buf, "\"password\":\"");
    
    if (ssid_start) {
        ssid_start += 8;  // Move past "ssid":" to the start of the value
        char *ssid_end = strchr(ssid_start, '\"');
        if (ssid_end) {
            int len = ssid_end - ssid_start;
            if (len < 33) {
                strncpy(ssid, ssid_start, len);
            }
        }
    }
    
    if (pass_start) {
        pass_start += 12;  // Move past "password":" to the start of the value
        char *pass_end = strchr(pass_start, '\"');
        if (pass_end) {
            int len = pass_end - pass_start;
            if (len < 64) {
                strncpy(password, pass_start, len);
            }
        }
    }
    
    if (strlen(ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    
    // Save to NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_str(nvs_handle, "ssid", ssid);
        nvs_set_str(nvs_handle, "password", password);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "WiFi credentials saved to NVS");
    }
    
    // Reset retry counter for new connection attempt
    wifi_retry_count = 0;
    
    // Configure WiFi station
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, password);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    // Switch to AP+STA mode
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    
    // Register event handler if not already registered
    static bool event_handler_registered = false;
    if (!event_handler_registered) {
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
        event_handler_registered = true;
    }
    
    esp_wifi_connect();
    
    httpd_resp_sendstr(req, "Connecting to WiFi... Check status in a few seconds");
    return ESP_OK;
}

// Start web server
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP server on port: %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t status_uri = {
            .uri = "/api/status",
            .method = HTTP_GET,
            .handler = status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &status_uri);

        httpd_uri_t battery_reset_uri = {
            .uri = "/api/battery/reset",
            .method = HTTP_POST,
            .handler = battery_reset_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &battery_reset_uri);

        httpd_uri_t motor_start_uri = {
            .uri = "/api/motor/start",
            .method = HTTP_POST,
            .handler = motor_start_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &motor_start_uri);

        httpd_uri_t motor_stop_uri = {
            .uri = "/api/motor/stop",
            .method = HTTP_POST,
            .handler = motor_stop_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &motor_stop_uri);

        httpd_uri_t motor_speed_uri = {
            .uri = "/api/motor/speed",
            .method = HTTP_POST,
            .handler = motor_speed_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &motor_speed_uri);

        httpd_uri_t motor_protocol_uri = {
            .uri = "/api/motor/protocol",
            .method = HTTP_POST,
            .handler = motor_protocol_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &motor_protocol_uri);

        httpd_uri_t ota_update_uri = {
            .uri = "/api/ota/update",
            .method = HTTP_POST,
            .handler = ota_update_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &ota_update_uri);

        httpd_uri_t wifi_status_uri = {
            .uri = "/api/wifi/status",
            .method = HTTP_GET,
            .handler = wifi_status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_status_uri);

        httpd_uri_t wifi_scan_uri = {
            .uri = "/api/wifi/scan",
            .method = HTTP_GET,
            .handler = wifi_scan_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_scan_uri);

        httpd_uri_t wifi_connect_uri = {
            .uri = "/api/wifi/connect",
            .method = HTTP_POST,
            .handler = wifi_connect_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_connect_uri);

        httpd_uri_t wifi_clear_uri = {
            .uri = "/api/wifi/clear",
            .method = HTTP_POST,
            .handler = wifi_clear_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_clear_uri);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

extern "C" void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-C6 Service Bench Starting (ESP-IDF)");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize PWM for ESC motor control - start with Standard PWM protocol
    configure_protocol(PROTOCOL_STANDARD);
    
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = MOTOR_PWM_GPIO,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = MOTOR_PWM_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = MOTOR_PWM_TIMER,
        .duty           = 0, // Start with motor off
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    
    ESP_LOGI(TAG, "ESC control initialized on GPIO%d - use /api/motor/protocol to switch protocols", MOTOR_PWM_GPIO);
    
    // Check if BOOT button (GPIO9) is pressed at startup to clear WiFi credentials
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_9);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    
    vTaskDelay(pdMS_TO_TICKS(100)); // Debounce
    
    if (gpio_get_level(GPIO_NUM_9) == 0) {  // Button pressed (active low)
        ESP_LOGW(TAG, "BOOT button pressed - Clearing WiFi credentials!");
        nvs_handle_t nvs_handle;
        if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
            nvs_erase_key(nvs_handle, "wifi_ssid");
            nvs_erase_key(nvs_handle, "wifi_pass");
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            ESP_LOGI(TAG, "WiFi credentials cleared!");
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Give user time to release button
    }
    
    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();  // Create STA interface for scanning

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handler for WiFi events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Configure AP
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, "ServiceBench");
    strcpy((char*)wifi_config.ap.password, "tech1234");  // Must be 8+ characters
    wifi_config.ap.ssid_len = strlen("ServiceBench");
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (strlen("tech1234") == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Start in APSTA mode to allow scanning while AP is active
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP Started Successfully!");
    ESP_LOGI(TAG, "SSID: ServiceBench");
    ESP_LOGI(TAG, "Password: tech1234");
    ESP_LOGI(TAG, "IP Address: 192.168.4.1");

    // Start web server
    httpd_handle_t server = start_webserver();
    if (server) {
        ESP_LOGI(TAG, "Web server started!");
        ESP_LOGI(TAG, "Open http://192.168.4.1 in your browser");
    }

    // Update sensor data periodically
    while(1) {
        // Simulate sensor readings
        battery_voltage = 12.0 + (rand() % 15) * 0.1;
        if (motor_rpm > 0) {
            motor_rpm = 3000 + (rand() % 1000);
        }
        
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
