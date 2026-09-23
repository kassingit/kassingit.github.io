//==== source ~/.espressif/tools/activate_idf_v6.1.sh
//==== idf.py menuconfig
//==== Build: idf.py build
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <driver/i2c_master.h>
#include <esp_log.h>
#include "esp_rom_sys.h"
#include "nvs_flash.h"           // pour nvs_flash_init(), nvs_flash_erase()
#include "nimble/nimble_port.h"  // pour nimble_port_init()
#include "nimble/nimble_port_freertos.h"  // pour nimble_port_freertos_init() et _deinit()
#include "host/ble_hs.h"                   // pour ble_hs_cfg
#include "services/gap/ble_svc_gap.h"    // pour ble_svc_gap_init()
#include "services/gatt/ble_svc_gatt.h"  // pour ble_svc_gatt_init()
#include <string.h>

//============================ KEYPAD ZONE =========================================

static const char *KEYPAD_TAG = "[ KEYPAD ]:";
// GPIO des lignes (sorties)
#define ROW1_GPIO GPIO_NUM_12
#define ROW2_GPIO GPIO_NUM_13
#define ROW3_GPIO GPIO_NUM_14
#define ROW4_GPIO GPIO_NUM_32

// GPIO des colonnes (entrées avec pull-up)
#define COL1_GPIO GPIO_NUM_0
#define COL2_GPIO GPIO_NUM_5
#define COL3_GPIO GPIO_NUM_18
#define COL4_GPIO GPIO_NUM_19
static const gpio_num_t row_pins[4] = {ROW1_GPIO,ROW2_GPIO,ROW3_GPIO,ROW4_GPIO};
static const gpio_num_t col_pins[4]= {COL1_GPIO,COL2_GPIO,COL3_GPIO,COL4_GPIO};

// MAP OF THE KEYPAD
static const char keymap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

void keymap_gpio_init(void){

    // ROWS CONFIGURATION IN OUPUT DIRECTION , AND HIGH STATE
    for (int i = 0; i <4; i++){
        gpio_reset_pin(row_pins[i]);
        gpio_set_direction(row_pins[i],GPIO_MODE_OUTPUT);
        gpio_set_level(row_pins[i],1);
    }

    // LINES CONFIGURATION IN INPUT DIRECTION WITH INTERNAL PULL-UP RESISTANCE
    for (int i = 0; i <4; i++){
        gpio_reset_pin(col_pins[i]);
        gpio_set_direction(col_pins[i],GPIO_MODE_INPUT);
        gpio_set_pull_mode(col_pins[i],GPIO_PULLUP_ONLY);
    }
        ESP_LOGI(KEYPAD_TAG," KEYPAD INITIALIZED !");
}

        //============= CHARACTER DETECTION

char keypad_scan(void){

    char key_pressed = 0;

    for (int r = 0; r<4; r++){
        for (int i =0; i<4;i++){
            gpio_set_level(row_pins[i],1);
        }
        
        // THE ROW TO TEST
        gpio_set_level(row_pins[r],0);

        vTaskDelay(pdMS_TO_TICKS(1));

        for (int c = 0; c<4; c++)
        {
            //ESP_LOGI(KEYPAD_TAG, "Test ligne %d, colonne %d = %d", r, c, gpio_get_level(col_pins[c]));
            if(gpio_get_level(col_pins[c])==0)
            { 
                // ANTI-BOUNCE
                vTaskDelay(pdMS_TO_TICKS(20));

                if(gpio_get_level(col_pins[c])==0)
                {
                    key_pressed = keymap[r][c];

                    while(gpio_get_level(col_pins[c])==0)
                    {
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                }
            }
        }
    }
    return key_pressed;
}




//==================================== I2C CREATION ================================================

// SDA AND SCL PINS DEFINITION
#define I2C_MASTER_SDA_IO GPIO_NUM_21
#define I2C_MASTER_SCL_IO GPIO_NUM_22

// THE I2C CONTROLLER CHOICE
#define I2C_MASTER_NUM I2C_NUM_0

// FREQUENCY DEFINITION (SCL)
#define I2C_MASTER_FREQ_HZ 100000

static const char *TAG = "[ I2C_DRIVER ]:";
static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t lcd_handle;

esp_err_t i2c_master_init(void){
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true
    };

    esp_err_t err = i2c_new_master_bus(&i2c_bus_config, &bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FAIL TO CREATE THE I2C DRIVER: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C BUS SUCCESSFULLY CREATED (SDA = %d, SCL = %d)", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    return ESP_OK;
}

// FUNCTION FOR SCANNING THE CONNECTED DEVICES AND GET THEIR ADDRESS
void addressScan(void){
    esp_err_t val;
    for (int i = 0x08; i <= 0x77; i++) {
        val = i2c_master_probe(bus_handle, i, 50);
        if (val == ESP_OK) {
            ESP_LOGI(TAG, "DEVICE FOUND AT THE ADDRESS :%02X", i);
        }
    }
}

// TO SEND NIBBLE (4 BITS)
void lcd_write_nibble(uint8_t nibble, uint8_t rs)
{
    // BL=1, E=1, RW=0
    uint8_t octet_E1 = (nibble << 4) | (1 << 3) | (1 << 2) | rs;
    uint8_t octet_E0 = (nibble << 4) | (1 << 3) | rs; // E=0, déclenche la lecture

    uint8_t buffer1[1] = {octet_E1};
    uint8_t buffer2[1] = {octet_E0};

    i2c_master_transmit(lcd_handle, buffer1, 1, 100);
    esp_rom_delay_us(100);
    i2c_master_transmit(lcd_handle, buffer2, 1, 100);
    esp_rom_delay_us(100);
}

// SEND A BYTE
void lcd_send_byte(uint8_t byte, uint8_t rs){
    uint8_t first_nibble = (byte >> 4);
    lcd_write_nibble(first_nibble, rs);

    esp_rom_delay_us(100);

    uint8_t second_nibble = byte & 0x0F;
    lcd_write_nibble(second_nibble, rs);
}

void lcd_init(void){
    esp_rom_delay_us(50000); // 50ms, attente après power-on

    lcd_write_nibble(0x03, 0);
    esp_rom_delay_us(4500);

    lcd_write_nibble(0x03, 0);
    esp_rom_delay_us(4500);

    lcd_write_nibble(0x03, 0);
    esp_rom_delay_us(150);

    lcd_write_nibble(0x02, 0); // Passage en mode 4 bits

    lcd_send_byte(0x28, 0); // Function Set : 4 bits, 2 lignes, 5x8
    lcd_send_byte(0x0C, 0); // Display ON, curseur OFF, blink OFF

    lcd_send_byte(0x01, 0); // Clear Display
    esp_rom_delay_us(2000); // Délai supplémentaire, cette commande est lente

    lcd_send_byte(0x06, 0); // Entry Mode Set : incrément auto du curseur
}


void keypad_task(void *pvParameterS){
    keymap_gpio_init();

    while(1){
        char key = keypad_scan();
        if(key!=0){
            ESP_LOGI(KEYPAD_TAG,"PRESSED TOUCH : %c",key);
                lcd_send_byte(key, 1); // rs=1 pour les données
                esp_rom_delay_us(100);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


//========================================== BLE ==================================================
static const char* BLE_TAG = "[ BLE ]:";

//NOM DE L'APPAREIL ET ADVERTISING
void ble_app_on_sync(void){
    ESP_LOGI(BLE_TAG,"Stack NimBLE synchronisée, prête à démarrer");
    ble_svc_gap_device_name_set("ESP32_SECURITY_SYSTEM");

    struct ble_hs_adv_fields fields;
    memset(&fields,0,sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)"ESP32_SECURITY_SYSTEM";
    fields.name_len = strlen("ESP32_SECURITY_SYSTEM");
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params;
    memset(&adv_params,0,sizeof(adv_params));

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC,NULL,BLE_HS_FOREVER,&adv_params,NULL,NULL);
}


void ble_host_task(void *param){
    nimble_port_run(); //   cette fonction de retourne jamais tant que nimble tourne
    nimble_port_freertos_deinit();    
}

#define GATT_SVC_UUID 0x0FFF // UUID du service "SAFETY"
#define GATT_CHR_CODE_UUID 0xFF01 // UUID de la caractéristique "code entré"

#define BLE_OP_READ 0
#define BLE_OP_WRITE 1

static int ble_svc_access_cb(uint16_t connect_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    switch(ctxt->op){
        case BLE_OP_READ :
            // LE LIENT DEMANDE À LIRE LA CARACTÉRISTIQUE
            ESP_LOGI(BLE_TAG,"LECTURE DEMANDÉE PAR LE CLIENT, N°: %d",ctxt->op);
            break;

        case BLE_OP_WRITE:
            //LE CLIENT DEMANDE À ECRIRE LA CARACTÉRISTIQUE
            ESP_LOGI(BLE_TAG,"LE CLIENT DEMANDE À ÉCRIRE LA CARACTÉRISTIQUE, N°: %d",ctxt->op);
            break;

        default:
            ESP_LOGI(BLE_TAG,"OPERATION INCUNNUE N°:%d",ctxt->op);
            break;
    }
    return 0;
};

//STRUCTURE DE LA CARACTÉRISTIQUE 
static const struct ble_gatt_chr_def gatt_chr_code[] = {
    {
        .uuid = BLE_UUID16_DECLARE(GATT_CHR_CODE_UUID), // ON AJOUTE LA CARACTÉRISTIQUE(LA DONNÉE) AU SERVICE( ENSEMBLE DE CARACTÉRISTIQUES)
        .access_cb = ble_svc_access_cb,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
    },
    {
        0,// MARQEUR DE FIN DE TABLEAU OBLIGATOIRE
    }
};

//STRUCTURE DU SERVICE 
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_SVC_UUID),
        .characteristics = gatt_chr_code,
    },
    {
        0,
    }
};


// SERVEUR BLEUTOOTH
void ble_gatt_svr_init(void){

    ble_svc_gap_init(); // CONTIENT LE NOM DE L'APPAREIL
    ble_svc_gatt_init(); // CONTIENT LES META-INFOS SUR LES SERVICES DISPONIBLES

    ble_gatts_count_cfg(gatt_svcs); //RESERVATION DES RESSOURCES POUR LE SERVICES gatt_svcs
    ble_gatts_add_svcs(gatt_svcs); // AJOUT DU SERVICE AU STACK GATT RENDANT LE SERVICE ACCESSIBLE

}

//=========================================== MAIN ===================================================
void app_main(void){
    ESP_ERROR_CHECK(i2c_master_init());
    addressScan();

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x27,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        .scl_wait_us = 1000
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &lcd_handle));
    ESP_LOGI(TAG, "LCD device added to I2C bus");

    lcd_init();

    esp_rom_delay_us(10000);

    const char* message = "CODE: ";
    for (int i = 0; message[i] != '\0'; i++) {
        lcd_send_byte(message[i], 1); // rs=1 pour les données
        esp_rom_delay_us(100);
    }
    xTaskCreate(keypad_task,"KEYPAD_TASK",2048,NULL,5,NULL);

    //========= BLE    
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
    nimble_port_init(); // démarre les structures internes de la stack Bluetooth (mémoire, files d'événements internes, etc.)

    ble_hs_cfg.sync_cb = ble_app_on_sync;
    ble_gatt_svr_init();
    nimble_port_freertos_init(ble_host_task);
}
