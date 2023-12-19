/*
Luis Miguel Santurino Perales
Cabezal de Riego Inteligente
2023
Usando espressif + plataforma Thingsboard 
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"

#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "gpio.h"
#include "driver/adc.h"

#include "esp_log.h"
#include "time.h"
#include "ssd1306.h"
#include "esp_sleep.h"

#define TAG "SSD1306"
#define TAG1 "WIFI"
#define TAG2 "MQTT"
#define TAG3 "ADC1"
#define TAG4 "ADC2"
#define TAG5 "ADC3"
#define TAG6 "ADC4"
#define TAG7 "ELECTROVALVULA"
#define TAG8 "DEEP_SLEEP"
#define TAG9 "EY"
#define TAG10 "CAUDALIMETRO"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define EXAMPLE_ESP_WIFI_SSID "YOUR_SSID"
#define EXAMPLE_ESP_WIFI_PASS  "YOUR_PASSWORD"
#define relay 27 // pin de la electrovalvula

//CAUDALIMETRO
volatile long waterFlow=0.0; //volatile para indicar que puede cambiar en una interrupcion
float volume=0.0;
//PRESIONES
float presion= 0.0;
float presion2 = 0.0;
float presion3= 0.0;
float presion4 =0.0;
//MQTT
esp_mqtt_client_handle_t mqtt_client=NULL;
//WIFI
static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;
//ADC PINES Y CANALES PRESIONES
adc1_channel_t sensorPresion_1 = ADC1_CHANNEL_7; //pin 35
adc1_channel_t sensorPresion_2 = ADC1_CHANNEL_5; //pin 33
adc1_channel_t sensorPresion_3 = ADC1_CHANNEL_4; //pin 32
adc1_channel_t sensorPresion_4 = ADC1_CHANNEL_6; //pin 34

//DEEP SLEEP
static RTC_DATA_ATTR struct timeval sleep_enter_time;


//PANTALLA https://www.mischianti.org/2021/07/14/ssd1306-oled-display-draw-images-splash-and-animations-2/
SSD1306_t dev;
uint8_t barra_llena[] = {
		0b11111111, 0b11111111, 0b11111111, 0b11111111,
		0b11111111, 0b11111111, 0b11111111, 0b11111111,
		0b11111111, 0b11111111, 0b11111111, 0b11111111,
		0b11111111, 0b11111111, 0b11111111, 0b11111111,
		0b11111111, 0b11111111, 0b11111111, 0b11111111,
		0b11111111, 0b11111111, 0b11111111, 0b11111111,
		0b11111111, 0b11111111, 0b11111111, 0b11111111,
		0b11111111, 0b11111111, 0b11111111, 0b11111111,
		0b11111111, 0b11111111, 0b11111111, 0b11111111,
		0b11111111, 0b11111111, 0b11111111, 0b11111111,
		0b11111111, 0b11111111, 0b11111111, 0b11111111,
		0b11111111, 0b11111111, 0b11111111, 0b11111111
};
uint8_t barra_vacia[] = {
		0b00000000, 0b00000000, 0b00000000, 0b00000000,
		0b00000000, 0b00000000, 0b00000000, 0b00000000,
		0b00000000, 0b00000000, 0b00000000, 0b00000000,
		0b00000000, 0b00000000, 0b00000000, 0b00000000,
		0b00000000, 0b00000000, 0b00000000, 0b00000000,
		0b00000000, 0b00000000, 0b00000000, 0b00000000,
		0b00000000, 0b00000000, 0b00000000, 0b00000000,
		0b00000000, 0b00000000, 0b00000000, 0b00000000,
		0b00000000, 0b00000000, 0b00000000, 0b00000000,
		0b00000000, 0b00000000, 0b00000000, 0b00000000,
		0b00000000, 0b00000000, 0b00000000, 0b00000000,
		0b00000000, 0b00000000, 0b00000000, 0b00000000,
		0b00000000, 0b00000000, 0b00000000, 0b00000000
};


uint8_t circulo[] = {
	
    
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000001, 0b11111111, 0b11111111, 0b11110000,
    0b00000011, 0b11111111, 0b11111111, 0b11111000,
    0b00000111, 0b11111111, 0b11111111, 0b11111100,
    0b00000111, 0b11111111, 0b11111111, 0b11111100,
    0b00000111, 0b11111111, 0b11111111, 0b11111100,
    0b00000011, 0b11111111, 0b11111111, 0b11111000,
    0b00000001, 0b11111111, 0b11111111, 0b11110000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000

};

int generarNumeroAleatorio() {
    // Establecer la semilla para la generación de números aleatorios
    srand(time(NULL));
    
    // Generar un número aleatorio en el rango de 0 a 55 (50 - (-5) + 1)
    int numeroAleatorio = rand() % 5;
    
    return numeroAleatorio;
}


/*-----------------------------WIFI MODO STA-------------------------------*/
static void event_handler(void* arg, esp_event_base_t event_base,  int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 5) {
            esp_wifi_connect(); // Intenta reconectar el wifi hasta el maximo numero de intentos.
            s_retry_num++;
            ESP_LOGI(TAG1, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT); // Si se acaban los intenos, gg.
        }
        ESP_LOGI(TAG1,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG1, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        //ESP_LOGI(TAG, "station %s join, AID=%d", MAC2STR(event->mac), event->aid);
		ESP_LOGI(TAG1, "ME CONECTOOOOOOOOOOOOOOOOOOOO.");
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        /* ESP_LOGI(TAG, "station %s leave, AID=%d",
                 MAC2STR(event->mac), event->aid); */
				 ESP_LOGI(TAG1, "ME DESCONECTOOOOOO DE WIFI.");
    }
	
}
void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate(); // Crear grupo de eventos del WIFI.

    ESP_ERROR_CHECK(esp_netif_init()); // Inicializa el TCP/IP. LLamar una vez en la app.
	

    ESP_ERROR_CHECK(esp_event_loop_create_default()); // Crea un loop de eventos por defecto.
    esp_netif_create_default_wifi_sta(); // Crea una configuracion para la estacion WiFi por defecto. Devuelve un puntero a la config por defecto.

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // Parametros por defecto para la inicializacion del WiFi.
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // Inicia el wifi con los parametros por defecto.

    esp_event_handler_instance_t instance_any_id; // Identificador de un Event Handler.
    esp_event_handler_instance_t instance_got_ip; // Same
	
	// Registra la funcion event_handler como handler para los eventos any_id y got ip mas abajo.
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = WIFI_AUTH_OPEN,
            .sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK,
            //.sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) ); //4 modos, stacion, ap, stacion + ap y NAN.
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) ); // Configura el wifi con la configuracion y el modo seleccionados. OJO se guarda en NVS for station and soft-AP
    ESP_ERROR_CHECK(esp_wifi_start() ); // Inicia el WIFI 

    ESP_LOGI(TAG1, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	 // ESPERA ACTIVA HASTA QUE COGEN VALOR LOS BITS DE CONECTADO O FALLIDOS.
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
	 // LOG PARA DECIR QUE HA SUCEDIDO.
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG1, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG1, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG1, "UNEXPECTED EVENT");
    }
}

/*-------------------------------------MODO MQTT------------------------------------------------*/
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG2, "Last error %s: 0x%x", message, error_code);
    }
}

static void process_received_data(const char *topic, const char *data, int data_len)
{
    ESP_LOGI(TAG2, "Mensaje recibido en el topic: %s", topic);
    ESP_LOGI(TAG2, "Datos: %.*s", data_len, data);

    // Aquí debes analizar el topic y los datos recibidos para realizar la acción correspondiente
    
    if (strstr(data, "true") != NULL) {
        // Cambiar el GPIO para encender el interruptor
        ESP_LOGI(TAG7, "Interruptor encendido");
		gpio_set_level(relay,1);
    } else if (strstr(data, "false") != NULL) {
        // Cambiar el GPIO para apagar el interruptor
        ESP_LOGI(TAG7, "Interruptor apagado");
		gpio_set_level(relay,0);
    } else if(strstr(data, "2") != NULL)
	{
		ESP_LOGI(TAG8, "Sistema Apagado");
		gpio_set_level(relay,0);
		esp_deep_sleep_start();
	}
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG2, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG2, "MQTT_EVENT_CONNECTED");
		msg_id=esp_mqtt_client_subscribe(mqtt_client, "v1/devices/me/rpc/request/+", 0);
           ESP_LOGI(TAG, "Suscripción a atributos, ID de mensaje: %d", msg_id);
		char message [30];
		int ramdom=generarNumeroAleatorio;
		sprintf(message, "{ presion: %d}", ramdom);
        msg_id = esp_mqtt_client_publish(client, "v1/devices/me/telemetry", message, 0, 1, 0);
		ramdom=generarNumeroAleatorio;
		sprintf(message, "{ caudalimetro: %d}", ramdom);
        msg_id = esp_mqtt_client_publish(client, "v1/devices/me/telemetry", message, 0, 1, 0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG2, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG2, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG2, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG2, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        break;
    case MQTT_EVENT_DATA:
		process_received_data(event->topic, event->data, event->data_len); //Esta funcion es la que cambia el estado de la electrovalvula proveniente de thingsboard
        break;
    default:
        ESP_LOGI(TAG2, "Other event id:%d", event->event_id);
        break;
    }
}


static void mqtt_app_start(void)
{	
	//Configure with your settings
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://TOKEN@demo.thingsboard.io:1883",//mqtt://mqtt.thingsboard.cloud"
		.credentials.username = "XXXXXX",
		.credentials.client_id = "XXXXXXXXX",           
		.credentials.authentication.password = "XXXXXXXXXXXX"
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL); // Registra un evento ESP_EVENT_ANY_ID
    esp_mqtt_client_start(mqtt_client);
}


//----------------------------------TAREAS-------------------------------------------//

//Esta funcion envia datos a thingsboard
void tarea_enviar_datos_mqtt(void *parameters) {
	TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(5000); //CADA 5 SEGUNDO SE ENVIARAN DATOS

    xLastWakeTime = xTaskGetTickCount();
	char message [30];

    while (1) {
		
		
		sprintf(message, "{ presion: %f}", presion);
        esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", message, 0, 1, 0);
		ESP_LOGI(TAG2, "Envío dato presion1");
		sprintf(message, "{ presion2: %f}", presion2);
        esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", message, 0, 1, 0);
		ESP_LOGI(TAG2, "Envío dato presion2");
		sprintf(message, "{ presion3: %f}", presion3);
        esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", message, 0, 1, 0);
		ESP_LOGI(TAG2, "Envío dato presion3");
		sprintf(message, "{ presion4: %f}", presion4);
        esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", message, 0, 1, 0);
		ESP_LOGI(TAG2, "Envío dato presion 4");
		sprintf(message, "{ caudalimetro: %f}", volume);
		esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", message, 0, 1, 0);
		ESP_LOGI(TAG2, "Envío dato caudalimetro");
		
       
        // Espera hasta que sea el momento de ejecutar la tarea nuevamente
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
		
    }
}


void tarea_enviar_datos_pantalla(void *parameters)
{
	
	char principio[25] = " Waterflow";
	int valor;
	int y=16;
	
	while(1)
	{
		sprintf(principio," Waterflow %fmL",volume); //almacena en principio, la cadena + el valor de volume
		ssd1306_display_text(&dev,0,principio,25,false);
		ESP_LOGI(TAG,"%f",volume);
		
		if(volume<=25000)
			valor=0;
		else if(volume>25000 && volume<=50000)
			valor=1;
		else if(volume>50000 && volume<=75000)
			valor=2;
		else
			valor=3;
		
		
		switch(valor)
		{	
			case 0:
			case 1: 
				ssd1306_bitmaps(&dev,5, y ,barra_vacia,32,13,false);
				ssd1306_bitmaps(&dev,47,y,barra_vacia,32,13,false);
				ssd1306_bitmaps(&dev,89,y,barra_vacia,32,13,false);
				ssd1306_bitmaps(&dev,50,40,barra_vacia,32,13,false);
				break;
			case 2:
				ssd1306_bitmaps(&dev,5,y,barra_llena,32,13,false);
				ssd1306_bitmaps(&dev,47,y,barra_vacia,32,13,false);
				ssd1306_bitmaps(&dev,89,y,barra_vacia,32,13,false);
				ssd1306_bitmaps(&dev,50,40,barra_vacia,32,13,false);
				break;
			case 3:
				ssd1306_bitmaps(&dev,5,y,barra_llena,32,13,false);
				ssd1306_bitmaps(&dev,47,y,barra_llena,32,13,false);
				ssd1306_bitmaps(&dev,89,y,barra_vacia,32,13,false);
				ssd1306_bitmaps(&dev,50,40,barra_vacia,32,13,false);
				break;
			case 4:
				ssd1306_bitmaps(&dev,5,y,barra_llena,32,13,false);
				ssd1306_bitmaps(&dev,47,y,barra_llena,32,13,false);
				ssd1306_bitmaps(&dev,89,y,barra_llena,32,13,false);
				ssd1306_bitmaps(&dev,50,40,circulo,32,13,false);
			break;
		}
	vTaskDelay(300);
	}
	
	
}


void pulse(void *arg) {
    waterFlow ++;
}


void tarea_caudalimetro(void *parameters)
{

	while(1)
	{
	volume= waterFlow*2.62;
    printf("waterFlow: %lf   mL\n", volume);
	 vTaskDelay(5000 / portTICK_PERIOD_MS); //para que nos lo de cada 5s
	}
}

//CONFIGURAR PIN QUE SEA EL CAUDALIMETRO
void configuracion_caudalimetro_electrovalvula()
{
	gpio_set_direction(relay,GPIO_MODE_OUTPUT); //electrovalvula el 27
	gpio_config_t interruptConfig = {
        .pin_bit_mask = (1ULL<<26),       // Pin digital 26
        .mode = GPIO_MODE_INPUT,          //entrada
        .intr_type = GPIO_INTR_POSEDGE  // interrupcion en flanco de subida
    };
     ESP_ERROR_CHECK(gpio_config(&interruptConfig)); //configuracion de la interrupt
     ESP_ERROR_CHECK(gpio_install_isr_service(0));
	gpio_isr_handler_add(26, pulse, (void *)26);
	
	
}

esp_err_t init_adc(adc1_channel_t sensor1,adc1_channel_t sensor2,adc1_channel_t sensor3,adc1_channel_t sensor4)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(sensor1, ADC_ATTEN_DB_11);
	adc1_config_channel_atten(sensor2, ADC_ATTEN_DB_11);
	adc1_config_channel_atten(sensor3, ADC_ATTEN_DB_11);
	adc1_config_channel_atten(sensor4, ADC_ATTEN_DB_11);
	
    return ESP_OK;
}

float getValor_adc1 (adc1_channel_t sensor1)
{
    float adc_sensor1 = adc1_get_raw(sensor1);
	float valor;
	//ESP_LOGI(TAG7, " Este es el valor del adc_sensor1: %f " , adc_sensor1);
	if(adc_sensor1<=500.0)
			valor=0;
	else
    valor = ((adc_sensor1 * 12) / 4095)*0.4336;
    return valor;
}

float getValor_adc2 (adc1_channel_t sensor2)
{
    float adc_sensor2 = adc1_get_raw(sensor2);
	float valor;
	//ESP_LOGI(TAG7, " Este es el valor del adc_sensor2: %f " , adc_sensor2);
	if(adc_sensor2<=530.0)
			valor=0;
	else
    valor = ((adc_sensor2 * 12) / 4095)*0.4336;
    return valor;
}

float getValor_adc3 (adc1_channel_t sensor3)
{
	float adc_sensor3 = adc1_get_raw(sensor3);
	float valor;
	//ESP_LOGI(TAG7, " Este es el valor del adc_sensor3: %f " , adc_sensor3);
	if(adc_sensor3<=515.0)
			valor=0;
	else
    valor = ((adc_sensor3 * 12) / 4095)*0.2914;
	
    return valor;
}

float getValor_adc4 (adc1_channel_t sensor4)
{
    float adc_sensor4 = adc1_get_raw(sensor4);
	float valor;
	//ESP_LOGI(TAG7, " Este es el valor del adc_sensor4: %f " , adc_sensor4);
	if(adc_sensor4<=510.0)
			valor=0;
	else
    valor = ((adc_sensor4 * 12) / 4095)*0.4;
    return valor;
}
//si adc_sensor es menor a 470.0 lo ponemos a 0
// El primero y segundo por 0.4366  
// El tercero
// El cuarto 0.4703

void tarea_presion(void *parameters)
{
	
	while(1)
    {
		presion=getValor_adc1(sensorPresion_1);
		presion2=getValor_adc2(sensorPresion_2);
		ESP_LOGI(TAG3,"%.3f bares presion1", presion);
		ESP_LOGI(TAG4,"%.3f bares presion2", presion2);
		
		presion3=getValor_adc3(sensorPresion_3);
		presion4=getValor_adc4(sensorPresion_4);
		ESP_LOGI(TAG5,"%.3f bares presion3", presion3);
		ESP_LOGI(TAG6,"%.3f bares presion4", presion4);
		if((presion-presion2)>0.5||(presion-presion2)<-0.5)
			ESP_LOGI(TAG3,"El sistema entre sensor1 y sensor2 está obstruido");
		if((presion3-presion4)>0.5||(presion3-presion4)<-0.5)
			ESP_LOGI(TAG6,"El sistema entre sensor3 y sensor4 tiene fugas");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
	
}


//----------------------------------------------------------------------------------------------//

void init_i2c_y_pantalla(){

//SDA 21 SCL 22 RESET 15 PUERTO 0
	i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
	ssd1306_init(&dev, 128, 64);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_clear_screen(&dev, false);
	ESP_LOGI(TAG,"Iniciado I2C y pantalla");
}


void parametros_dee_sle()
{
	//Configurar con los segundos que deseas para que el sistema permanezca dormido. Después se inicializa de nuevo.
	const int wakeup_time_sec = 20; // 20 segundos
	esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);
	
	
}



void app_main(void)
{
	//Inicializaciones
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
	parametros_dee_sle();
	configuracion_caudalimetro_electrovalvula();
	init_adc(sensorPresion_1,sensorPresion_2,sensorPresion_3,sensorPresion_4); 
	wifi_init_sta();
	init_i2c_y_pantalla();
	mqtt_app_start();
	
	
	xTaskCreate(tarea_presion,"tarea_presion", 8192,NULL,7,NULL);
	xTaskCreate(tarea_caudalimetro, "tarea_caudalimetro",8192,NULL, 2, NULL);
	xTaskCreate(tarea_enviar_datos_mqtt, "tarea_enviar_datos_mqtt", 8192, NULL, 5, NULL);
	xTaskCreate(tarea_enviar_datos_pantalla, "tarea_enviar_datos_pantalla",8192,NULL,4,NULL);
	
	while(1);

	

}










    
