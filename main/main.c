#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"

// High Freqeuncy Switch pin
#define Q1 16
#define Q2 17
#define Q3 18
#define Q4 19

// PWM Funct Declaration
mcpwm_timer_handle_t timer;
mcpwm_oper_handle_t operators[3];
mcpwm_cmpr_handle_t comparators[3];
mcpwm_gen_handle_t generators[4];
mcpwm_dead_time_config_t dt_config_1;

void MCPWM_INIT(void);
void SYNC_PWM(void);
static const char *TAG = "PFSB"; 

int delay_add = 0;
int L = 0;
int K = 0;

void IRAM_ATTR DeadTimeUpdate(){
    L++;
    if(L == 1000){
    L = 0;
        if(delay_add < 780){
            delay_add++;
            dt_config_1 = (mcpwm_dead_time_config_t) {
                .posedge_delay_ticks = delay_add + 10,
                .negedge_delay_ticks = delay_add,
                .flags.invert_output = false,
            };
            ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generators[2],generators[2], &dt_config_1));
            dt_config_1 = (mcpwm_dead_time_config_t) {
                .posedge_delay_ticks = delay_add,
                .negedge_delay_ticks = delay_add + 10,
                .flags.invert_output = true,
            };
            ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generators[3],generators[3], &dt_config_1));
        }
        else{
            delay_add = 0;
        }
    }
}

void MCPWM_INIT(void)
{
    ESP_LOGI(TAG, "PWM INITIALIZATION");
    // ====== One Timer Example Configuration ====== //
    ESP_LOGI(TAG, "Create timer and operator");
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT, // PLL Clock 160 MHz
        .resolution_hz = 160000000,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = 1600,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    // ====== One Operator to One Timer Example Configuration ====== //
    ESP_LOGI(TAG, "Create operators");
    mcpwm_operator_config_t operator_config = {
        .group_id = 0,
        .flags.update_dead_time_on_tez = true,
    };
    for(int i=0; i<=2; i++){
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &operators[i]));
    ESP_LOGI(TAG, "Connect operators to the same timer");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operators[i], timer));
    }

    // ====== Assign Comparator to One Operator Example Configuration ====== //
    ESP_LOGI(TAG, "Create comparators");
    mcpwm_comparator_config_t compare_config = {
        .flags.update_cmp_on_tez = true,
    };
    for(int i=0; i<=2; i++){
    ESP_ERROR_CHECK(mcpwm_new_comparator(operators[i], &compare_config, &comparators[i]));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparators[i], 800));
    }

    // ====== Create Generators for PWM Pin Output ====== //
    ESP_LOGI(TAG, "Create generators");
    mcpwm_generator_config_t gen_config = {};
    const int gen_gpios[4] = {Q1,Q2,Q3,Q4};
    for(int i=0; i<=1; i++){
    gen_config.gen_gpio_num = gen_gpios[i];
    ESP_ERROR_CHECK(mcpwm_new_generator(operators[0], &gen_config, &generators[i]));
    }
    for(int i=2; i<=3; i++){
    gen_config.gen_gpio_num = gen_gpios[i];
    ESP_ERROR_CHECK(mcpwm_new_generator(operators[i-1], &gen_config, &generators[i]));
    }

    // ====== Generator Action Example ====== //
    ESP_LOGI(TAG, "Set generator action on timer and compare event");
    // PWM Start with HIGH State when Timer is 0 and LOW State when Comparators value is equal Timer, For MCPWM_TIMER_COUNT_MODE_UP
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generators[0],MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generators[0],MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparators[0], MCPWM_GEN_ACTION_LOW)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generators[1],MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generators[1],MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparators[0], MCPWM_GEN_ACTION_LOW)));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generators[2],MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generators[2],MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparators[1], MCPWM_GEN_ACTION_LOW)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generators[3],MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generators[3],MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparators[2], MCPWM_GEN_ACTION_LOW)));


    mcpwm_dead_time_config_t dt_config_1 = {
        .posedge_delay_ticks = 10,
        .negedge_delay_ticks = 0
    };
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generators[0],generators[0], &dt_config_1));
    dt_config_1 = (mcpwm_dead_time_config_t) {
        .posedge_delay_ticks = 0,
        .negedge_delay_ticks = 10,
        .flags.invert_output = true,
    };
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generators[1],generators[1], &dt_config_1));

    dt_config_1 = (mcpwm_dead_time_config_t) {
        .posedge_delay_ticks = delay_add + 10,
        .negedge_delay_ticks = delay_add,
        .flags.invert_output = false,
    };
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generators[2],generators[2], &dt_config_1));
    dt_config_1 = (mcpwm_dead_time_config_t) {
        .posedge_delay_ticks = delay_add,
        .negedge_delay_ticks = delay_add + 10,
        .flags.invert_output = true,
    };
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generators[3],generators[3], &dt_config_1));

    mcpwm_timer_event_callbacks_t timer_cbs = {
        .on_empty = (void*)DeadTimeUpdate,
        // .on_full
    };
    ESP_ERROR_CHECK(mcpwm_timer_register_event_callbacks(timer, &timer_cbs, NULL));  

    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
}

void app_main() 
{
    MCPWM_INIT();
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
}