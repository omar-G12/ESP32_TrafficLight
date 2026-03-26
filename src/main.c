#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <stdio.h>

#define ESP_INR_FLAG_DEFAULT 0

#define Green_pin
#define Red_pin
#define Yellow_pin

typedef enum
{
    Red,
    Yellow,
    Green,
} TrafficLight;

typedef enum
{
    Traffic_1,
    Traffic_2
} TrafficTurn;

struct ledstuff
{
    gpio_num_t pin;
    gpio_num_t otherLightPin;
    int blinkRatems;
    TrafficLight color;
};
typedef struct ledstuff ledstuffstruct;

TrafficLight current;  /// current Color that the light is on
gpio_num_t currentLight; //Current pin the light is on for the active Traffic Light
gpio_num_t waitingLight; // Current pin the light is on for the Waiting Traffic light
TrafficTurn currentTurn;    //Determines the current Traffic Light that is active now

int set_delay = 15000; ///Set this as 15 sec so we can have a total of 20 second duration for active lights 

// GPIO_0 is the button on our board
TaskHandle_t ISR = NULL;


void currentTrafficTurn(gpio_num_t pin, gpio_num_t pin2)
{
    if (currentTurn == Traffic_1)
    {
        currentLight = pin;
        waitingLight = GPIO_NUM_4;
    }
    else if (currentTurn == Traffic_2)
    {
        // Other side's turn
        currentLight = pin2;
        waitingLight = GPIO_NUM_25;
    }
}

void checkChange()
{ 
    if (currentTurn == Traffic_1)
    {
        currentTurn = Traffic_2;
    }
    else if (currentTurn == Traffic_2)
    {
        currentTurn = Traffic_1;
    }
}

void Lights(int delay)
{
    int delayNow = current == Green ? set_delay : delay; 
    gpio_set_level(currentLight, 1);
    vTaskDelay(delayNow / portTICK_PERIOD_MS);  /// delay amount of seconds till LED turn off 
    gpio_set_level(currentLight, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);  //1 second delay needed for interrupt to detect change 
}

void blinktask(void *parameters)
{
    ledstuffstruct *data = (ledstuffstruct *)parameters;
    gpio_num_t mypin = data->pin;
    gpio_num_t pin2 = data->otherLightPin;
    int delay = data->blinkRatems;
    TrafficLight currentColor = data->color;
    while (1)
    {
        if (currentColor == current)
        {
            currentTrafficTurn(mypin, pin2);   
            gpio_set_level(waitingLight, 1);   ///Represent the waiting light of traffic 
            Lights(delay);
        }
    }
}

//example of event handler in hardware idk what it's called
void IRAM_ATTR RED_irs_handler(void *arg)
{   //after red goes green 
    current = Green; 
}

void IRAM_ATTR Yellow_irs_handler(void *arg)
{
    // after yellow goes red
    checkChange(); ///When it switches to Red, change the current active traffic 
    current = Red;
}

void IRAM_ATTR Green_irs_handler(void *arg)
{
    // after green goes yellow
    current = Yellow;
}

void IRAM_ATTR button_isr_handler(void *arg)
{
   if(set_delay == 15000){
    set_delay = 5000; ///Set this as 5000 so we can have a total duration of 10 seconds for active light 
   } else {
    set_delay = 15000; 
   }
}

void setup_task(ledstuffstruct *ledinfo)
{
    // Create the task for the specified LED
    currentTurn = Traffic_1;  ///Setup the first active light 
    xTaskCreate(blinktask, "Blink Task", 2048, ledinfo, 5, NULL);
}

void app_main()
{

    current = Green;
    /// Setting up the PINS AND INTERRUPTS 
    // seting up the traffic lights  --> INPUT AND OUTPUT B/C WE ARE GOING TO USE OUTPUT FOR LED AND INPUT FOR INTERRUPT
    gpio_set_direction(GPIO_NUM_27, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(GPIO_NUM_26, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(GPIO_NUM_25, GPIO_MODE_INPUT_OUTPUT);

    /// other side of traffic lights
    gpio_set_direction(GPIO_NUM_19, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(GPIO_NUM_18, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_INPUT_OUTPUT);

    // ///////Any edge triggers
    gpio_set_intr_type(GPIO_NUM_26, GPIO_INTR_NEGEDGE);  //GPIO_INTR_NEGEDGE --> checks changes in LED (if it turns off)
    gpio_set_intr_type(GPIO_NUM_27, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(GPIO_NUM_25, GPIO_INTR_NEGEDGE);

    gpio_set_intr_type(GPIO_NUM_19, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(GPIO_NUM_18, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(GPIO_NUM_4, GPIO_INTR_NEGEDGE);

    // ///Interrupt stuff
    gpio_install_isr_service(ESP_INR_FLAG_DEFAULT);

    // esp_rom_gpio_pad_select_gpio(CONFIG_BUTTON_PIN);
    // esp_rom_gpio_pad_select_gpio(CONFIG_LED_PIN);

    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
    gpio_set_intr_type(GPIO_NUM_0, GPIO_INTR_NEGEDGE);

    gpio_isr_handler_add(GPIO_NUM_0, button_isr_handler, NULL);
    gpio_intr_enable(GPIO_NUM_0);

    ///////////////////////////////////////////////////////////////////

    ledstuffstruct *green_led_info = malloc(sizeof(ledstuffstruct));
    green_led_info->pin = GPIO_NUM_27;
    green_led_info->otherLightPin = GPIO_NUM_19;
    green_led_info->blinkRatems = set_delay;  //should be 5000 seconds when we toggle 
    green_led_info->color = Green;
    gpio_isr_handler_add(green_led_info->pin, Green_irs_handler, NULL);
    gpio_isr_handler_add(green_led_info->otherLightPin, Green_irs_handler, NULL);

    ledstuffstruct *yellow_led_info = malloc(sizeof(ledstuffstruct));
    yellow_led_info->pin = GPIO_NUM_26;
    yellow_led_info->otherLightPin = GPIO_NUM_18;
    yellow_led_info->blinkRatems = 2000;
    yellow_led_info->color = Yellow;
    gpio_isr_handler_add(yellow_led_info->pin, Yellow_irs_handler, NULL);
    gpio_isr_handler_add(yellow_led_info->otherLightPin, Yellow_irs_handler, NULL);

    ledstuffstruct *Red_led_info = malloc(sizeof(ledstuffstruct));
    Red_led_info->pin = GPIO_NUM_25;
    Red_led_info->otherLightPin = GPIO_NUM_4;
    Red_led_info->blinkRatems = 2000;
    Red_led_info->color = Red;
    gpio_isr_handler_add(Red_led_info->pin, RED_irs_handler, NULL);
    gpio_isr_handler_add(Red_led_info->otherLightPin, RED_irs_handler, NULL);

    gpio_intr_enable(GPIO_NUM_27);
    gpio_intr_enable(GPIO_NUM_26);
    gpio_intr_enable(GPIO_NUM_25);

    gpio_intr_enable(GPIO_NUM_19);
    gpio_intr_enable(GPIO_NUM_18);
    gpio_intr_enable(GPIO_NUM_4);

    setup_task(green_led_info);
    setup_task(yellow_led_info);
    setup_task(Red_led_info);

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}