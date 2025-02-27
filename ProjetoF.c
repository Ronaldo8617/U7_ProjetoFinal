#include <stdio.h>  
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "inc/ssd1306.h"
#include "inc/font.h"

// Definições dos pinos
#define JOYSTICK_X_PIN 26    // ADC0 – usado como "temperatura"
#define JOYSTICK_Y_PIN 27    // ADC1 – usado como "umidade"
#define MICROPHONE_PIN 28    // ADC2 – usado como "nível de ruído"
#define BUZZER_PIN 21        // Pino do buzzer
#define JOYSTICK_PB_PIN 22   // Botão do joystick (para override de ruído)
#define BUTTON_B_PIN 6       // Botão B (para imprimir os dados acumulados)
#define BUTTON_A_PIN 5       // Botão A (para atualizar os valores críticos)
#define LED_RED_PIN 13       // LED vermelho (PWM)
#define LED_GREEN_PIN 11     // LED verde (PWM)

// Definições do display OLED via I2C
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define OLED_ADDR 0x3C
#define WIDTH 128
#define HEIGHT 64

// Estrutura para armazenar os valores médios de cada "dia" (3 segundos)
typedef struct {
    float avg_temperature;
    float avg_humidity;
    float avg_noise;
} DayData;

#define MAX_DAYS 365  // Número máximo de "dias" armazenados

// Variáveis globais para acumulação de dados
DayData dayData[MAX_DAYS];
int day_count = 0;
uint64_t period_start;
int reading_count = 0;
float sum_temp = 0.0f, sum_hum = 0.0f, sum_noise = 0.0f;

// Variáveis globais para controle de override e impressão
volatile bool noise_override = false;
volatile bool print_daydata_flag = false;

// Variável para atualização dos valores críticos via botão A
volatile bool update_threshold_flag = false;

// Variáveis para debounce (200 ms)
volatile absolute_time_t last_interrupt_time_js;
volatile absolute_time_t last_interrupt_time_b;
volatile absolute_time_t last_interrupt_time_a;  // Para o botão A
#define DEBOUNCE_THRESHOLD_US 200000

// Variáveis dos limites críticos (valores padrões)
float crit_temp_low = 2.0f, crit_temp_high = 90.0f;
float crit_hum_low  = 10.0f, crit_hum_high  = 80.0f;
float crit_noise_low = 0.0f, crit_noise_high = 70.0f;

// Callback unificado para os botões (joystick, botão B e botão A)
void gpio_callback(uint gpio, uint32_t events) {
    absolute_time_t now = get_absolute_time();
    if (gpio == JOYSTICK_PB_PIN) {
        if (absolute_time_diff_us(last_interrupt_time_js, now) < DEBOUNCE_THRESHOLD_US)
            return;
        last_interrupt_time_js = now;
        noise_override = !noise_override;  // Alterna o override do ruído
    }
    else if (gpio == BUTTON_B_PIN) {
        if (absolute_time_diff_us(last_interrupt_time_b, now) < DEBOUNCE_THRESHOLD_US)
            return;
        last_interrupt_time_b = now;
        print_daydata_flag = true;  // Solicita a impressão do array dayData
    }
    else if (gpio == BUTTON_A_PIN) {
        if (absolute_time_diff_us(last_interrupt_time_a, now) < DEBOUNCE_THRESHOLD_US)
            return;
        last_interrupt_time_a = now;
        update_threshold_flag = true;  // Solicita a atualização dos valores críticos
    }
}

// Variáveis para PWM dos LEDs
uint red_slice, green_slice;
#define PWM_WRAP 1000  // Valor de wrap para o PWM

// Timer repetidor para atualizar os níveis de PWM dos LEDs (não bloqueante)
bool led_pwm_callback(struct repeating_timer *t) {
    static bool toggle = false;
    if (gpio_get(BUZZER_PIN)) {  // Se o buzzer está ativo
        if (toggle) {
            // LED vermelho em 100% e LED verde desligado
            pwm_set_gpio_level(LED_RED_PIN, PWM_WRAP);
            pwm_set_gpio_level(LED_GREEN_PIN, 0);
        } else {
            // LED vermelho em 50% e LED verde em aproximadamente 5%
            uint32_t level = (PWM_WRAP * 50) / 100;
            pwm_set_gpio_level(LED_RED_PIN, level);
            pwm_set_gpio_level(LED_GREEN_PIN, level/10);
        }
        toggle = !toggle;
    } else {
        // Se o buzzer não está ativo, ambos os LEDs ficam desligados
        pwm_set_gpio_level(LED_RED_PIN, 0);
        pwm_set_gpio_level(LED_GREEN_PIN, 0);
    }
    return true;  // Continua o timer repetidor
}

int main() {
    stdio_init_all();  // Inicializa a comunicação serial

    // Inicialização do I2C e do display OLED
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, OLED_ADDR, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicialização do ADC para os sensores
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);  // Temperatura
    adc_gpio_init(JOYSTICK_Y_PIN);  // Umidade
    adc_gpio_init(MICROPHONE_PIN);  // Nível de ruído

    // Inicialização do buzzer
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);

    // Configuração dos botões com pull-up
    gpio_init(JOYSTICK_PB_PIN);
    gpio_set_dir(JOYSTICK_PB_PIN, GPIO_IN);
    gpio_pull_up(JOYSTICK_PB_PIN);
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    // Registra o callback unificado para os botões
    gpio_set_irq_enabled_with_callback(JOYSTICK_PB_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true);

    // Inicializa as variáveis para o cálculo da média (3 segundos)
    period_start = time_us_64();

    // Configuração do LED vermelho para PWM
    gpio_set_function(LED_RED_PIN, GPIO_FUNC_PWM);
    red_slice = pwm_gpio_to_slice_num(LED_RED_PIN);
    pwm_set_wrap(red_slice, PWM_WRAP);
    pwm_set_gpio_level(LED_RED_PIN, 0);
    pwm_set_enabled(red_slice, true);

    // Configuração do LED verde para PWM
    gpio_set_function(LED_GREEN_PIN, GPIO_FUNC_PWM);
    green_slice = pwm_gpio_to_slice_num(LED_GREEN_PIN);
    pwm_set_wrap(green_slice, PWM_WRAP);
    pwm_set_gpio_level(LED_GREEN_PIN, 0);
    pwm_set_enabled(green_slice, true);

    // Configura um timer repetidor para atualizar o PWM dos LEDs (a cada 250 ms)
    struct repeating_timer led_timer;
    add_repeating_timer_ms(250, led_pwm_callback, NULL, &led_timer);
    
    while (true) {
        // Se o botão A foi pressionado, atualiza os valores críticos via Serial
        if (update_threshold_flag) {
            update_threshold_flag = false;
            printf("\n*** Atualizacao de Valores Criticos ***\n");
        
            printf("Entre com os novos valores para TEMPERATURA, UMIDADE e RUÍDO (min max min max min max):\n");
        
            float new_temp_low, new_temp_high;
            float new_hum_low, new_hum_high;
            float new_noise_low, new_noise_high;
            
            scanf("%f %f %f %f %f %f", 
                  &new_temp_low, &new_temp_high, 
                  &new_hum_low, &new_hum_high, 
                  &new_noise_low, &new_noise_high);
        
            crit_temp_low = new_temp_low;
            crit_temp_high = new_temp_high;
            crit_hum_low = new_hum_low;
            crit_hum_high = new_hum_high;
            crit_noise_low = new_noise_low;
            crit_noise_high = new_noise_high;
        
            printf("Novos valores críticos atualizados!\n");
            
            // Exibe os novos valores críticos no Serial Monitor:
            printf("Novos limites críticos:\n");
            printf("Temperatura: %.1f a %.1f\n", crit_temp_low, crit_temp_high);
            printf("Umidade:    %.1f a %.1f\n", crit_hum_low, crit_hum_high);
            printf("Ruído:      %.1f a %.1f\n\n", crit_noise_low, crit_noise_high);
        }
        // Leitura do ADC para temperatura (Joystick X)
        adc_select_input(0);
        uint16_t adc_x = adc_read();
        float temperature = (adc_x * 100.0f) / 4095.0f;

        // Leitura do ADC para umidade (Joystick Y)
        adc_select_input(1);
        uint16_t adc_y = adc_read();
        float humidity = (adc_y * 100.0f) / 4095.0f;

        // Leitura do ADC para nível de ruído (Microfone)
        adc_select_input(2);
        uint16_t adc_mic = adc_read();
        float noise_level = (adc_mic * 100.0f) / 4095.0f;

        // Se o override estiver ativo, força o nível de ruído para 100
        if (noise_override) {
            noise_level = 100.0f;
        }

        // Verifica condição crítica usando os limites configuráveis:
        if (temperature <= crit_temp_low || temperature >= crit_temp_high ||
            humidity    <= crit_hum_low  || humidity    >= crit_hum_high  ||
            noise_level <= crit_noise_low || noise_level >= crit_noise_high) {
            gpio_put(BUZZER_PIN, 1);
        } else {
            gpio_put(BUZZER_PIN, 0);
        }

        // Exibe os valores atuais no Serial Monitor
        printf("Temp: %.1f C, Hum: %.1f %%, Noise: %.1f%% %s\n",
               temperature, humidity, noise_level,
               noise_override ? "(Override)" : "");

        // Atualiza o display OLED com os valores atuais
        ssd1306_fill(&ssd, false);
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "Temp: %.1f C", temperature);
        ssd1306_draw_string(&ssd, buffer, 0, 10);
        snprintf(buffer, sizeof(buffer), "Hum:  %.1f %%", humidity);
        ssd1306_draw_string(&ssd, buffer, 0, 30);
        snprintf(buffer, sizeof(buffer), "Noise:%.1f %%", noise_level);
        ssd1306_draw_string(&ssd, buffer, 0, 50);
        ssd1306_send_data(&ssd);

        // Acumula as leituras para o cálculo da média
        sum_temp += temperature;
        sum_hum += humidity;
        sum_noise += noise_level;
        reading_count++;

        // Verifica se se passaram 3 segundos (3,000,000 µs)
        uint64_t now = time_us_64();
        if (now - period_start >= 3000000) {
            float avg_temp = sum_temp / reading_count;
            float avg_hum = sum_hum / reading_count;
            float avg_noise = sum_noise / reading_count;
            if (day_count < MAX_DAYS) {
                dayData[day_count].avg_temperature = avg_temp;
                dayData[day_count].avg_humidity = avg_hum;
                dayData[day_count].avg_noise = avg_noise;
                day_count++;
            }
            printf("Dia %d: Avg Temp: %.1f C, Avg Hum: %.1f %%, Avg Noise: %.1f%%\n",
                   day_count, avg_temp, avg_hum, avg_noise);
            period_start = now;
            reading_count = 0;
            sum_temp = 0.0f;
            sum_hum = 0.0f;
            sum_noise = 0.0f;
        }

        // Se o botão B foi pressionado, imprime o array dayData no Serial Monitor
        if (print_daydata_flag) {
            printf("\n=== Dados Acumulados (%d dias) ===\n", day_count);
            for (int i = 0; i < day_count; i++) {
                printf("Dia %d: Avg Temp: %.1f C, Avg Hum: %.1f %%, Avg Noise: %.1f%%\n",
                       i + 1,
                       dayData[i].avg_temperature,
                       dayData[i].avg_humidity,
                       dayData[i].avg_noise);
            }
            printf("===================================\n\n");
            print_daydata_flag = false;
        }

        sleep_ms(500);
    }

    return 0;
}