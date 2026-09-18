#include "hardware_sanity.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    nikos::HardwareSanity sanity;
    sanity.begin();

    while (true) {
        sanity.update();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
