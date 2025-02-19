#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/pwm.h"
#include "pico/bootrom.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define JOYSTICK_X_PIN 26  // GPIO para eixo X
#define JOYSTICK_Y_PIN 27  // GPIO para eixo Y
#define JOYSTICK_PB 22     // GPIO para botão do Joystick
#define Botao_A 5          // GPIO para botão A
#define Botao_B 6          // GPIO para botão B

// Pinos dos LEDs RGB
#define LED_RED_PIN   13  // LED Vermelho (eixo X do joystick)
#define LED_GREEN_PIN 11  // LED Verde (usado para acender/desligar com o botão do joystick)
#define LED_BLUE_PIN  12  // LED Azul (eixo Y do joystick)

// Para o controle de debouncing
#define DEBOUNCE_TIME 200  // Debouncing de 200ms

// Variáveis globais
volatile bool led_green_on = false;  // Estado do LED verde
volatile bool leds_enabled = true;  // Variável de controle para habilitar/desabilitar LEDs
volatile uint32_t last_interrupt_time_pb = 0;  // Armazena o último tempo do botão Joystick

// Função de interrupção para o Botão A, Botão B e o botão do joystick (GPIO 5, GPIO 6 e GPIO 22)
void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time - last_interrupt_time_pb > DEBOUNCE_TIME) {
        last_interrupt_time_pb = current_time;

        if (gpio == Botao_B) {  // Botão B foi pressionado (GPIO 6)
            reset_usb_boot(0, 0);  // Entra em BOOTSEL
        } else if (gpio == Botao_A) {  // Botão A (GPIO 5)
            leds_enabled = !leds_enabled;  // Alterna a ativação dos LEDs
            if (!leds_enabled) {
                pwm_set_gpio_level(LED_RED_PIN, 0);  // Desliga o LED vermelho
                pwm_set_gpio_level(LED_BLUE_PIN, 0); // Desliga o LED azul
            }
        } else if (gpio == JOYSTICK_PB) {  // Botão do Joystick (GPIO 22)
            // Alterna o estado do LED verde
            led_green_on = !led_green_on;

            // Acende ou apaga o LED verde dependendo do estado
            if (led_green_on) {
                pwm_set_gpio_level(LED_GREEN_PIN, 255);  // Acende o LED verde
            } else {
                pwm_set_gpio_level(LED_GREEN_PIN, 0);    // Apaga o LED verde
            }
        }
    }
}

void pwm_init_led(uint gpio_pin)
{
    // Inicializa o PWM para os LEDs
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio_pin);
    pwm_set_wrap(slice_num, 255);  // Define o valor máximo do PWM para 255
    pwm_set_enabled(slice_num, true);  // Habilita o PWM
}

int main()
{
    // Configuração para o modo BOOTSEL com o botão B
    gpio_init(Botao_B);  // GPIO 6 para o botão B
    gpio_set_dir(Botao_B, GPIO_IN);
    gpio_pull_up(Botao_B);
    gpio_set_irq_enabled_with_callback(Botao_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);  // Interrupção para o Botão B (GPIO 6)

    // Configura o botão A (GPIO 5)
    gpio_init(Botao_A);
    gpio_set_dir(Botao_A, GPIO_IN);
    gpio_pull_up(Botao_A);
    gpio_set_irq_enabled_with_callback(Botao_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);  // Interrupção para o Botão A (GPIO 5)

    // Inicializa o botão do joystick (GPIO 22) com interrupção
    gpio_init(JOYSTICK_PB);
    gpio_set_dir(JOYSTICK_PB, GPIO_IN);
    gpio_pull_up(JOYSTICK_PB); 
    gpio_set_irq_enabled_with_callback(JOYSTICK_PB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);  // Interrupção para o botão do Joystick

    // Inicialização do display I2C SSD1306
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); 
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); 
    gpio_pull_up(I2C_SDA); 
    gpio_pull_up(I2C_SCL); 
    ssd1306_t ssd; 
    ssd1306_init(&ssd, 128, 64, false, endereco, I2C_PORT); 
    ssd1306_config(&ssd); 
    ssd1306_send_data(&ssd); 

    // Limpa o display
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicialização do ADC para o joystick
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);  

    uint16_t adc_value_x;
    uint16_t adc_value_y;

    // Posições do quadrado
    int square_x = 64;  
    int square_y = 32;  
    int square_size = 8; 

    // Inicializa os PWM para os LEDs
    pwm_init_led(LED_RED_PIN);
    pwm_init_led(LED_BLUE_PIN);
    pwm_init_led(LED_GREEN_PIN); // LED verde para controle com o botão

    while (true)
    {
        adc_select_input(1); // Seleciona o ADC para eixo X. O pino 26 como entrada analógica
        adc_value_x = adc_read();
        adc_select_input(0); // Seleciona o ADC para eixo Y. O pino 27 como entrada analógica
        adc_value_y = adc_read();    

        // Mapeia o valor de Y para a tela de 0 a 63 (invertendo a direção)
        square_y = (4096 - adc_value_y) * 63 / 4095; 

        // Mapeia o valor de X para a tela de 0 a 127
        square_x = adc_value_x * 127 / 4095; 

        // Garante que o quadrado não ultrapasse os limites da tela
        if (square_x < 0) square_x = 0;
        if (square_x > 120) square_x = 120; // Subtrai o tamanho do quadrado para não ultrapassar a borda
        if (square_y < 0) square_y = 0;
        if (square_y > 56) square_y = 56; // Subtrai o tamanho do quadrado para não ultrapassar a borda

        // Limpa o display
        ssd1306_fill(&ssd, false);  
        // Desenha o quadrado em movimento
        ssd1306_rect(&ssd, square_y, square_x, square_size, square_size, true, false); // Quadrado em movimento
        ssd1306_send_data(&ssd); // Atualiza o display

        // Condições para os LEDs com base na posição do quadrado
        uint8_t led_red_pwm = 0;
        uint8_t led_blue_pwm = 0;

        // Intervalo de 8 pixels em torno do centro (±4 pixels)
        int center_x_min = 60;  
        int center_x_max = 68;  
        int center_y_min = 28;  
        int center_y_max = 36;  

        // Condição para o LED Vermelho (controle pelo eixo X do joystick)
        if (square_x < center_x_min || square_x > center_x_max) 
        {
            if (square_x < center_x_min) {
                led_red_pwm = (center_x_min - square_x) * 255 / center_x_min; // Movimento para a esquerda
            } else {
                led_red_pwm = (square_x - center_x_max) * 255 / (127 - center_x_max); // Movimento para a direita
            }
        }

        // Condição para o LED Azul (controle pelo eixo Y do joystick)
        if (square_y < center_y_min || square_y > center_y_max) 
        {
            if (square_y < center_y_min) {
                led_blue_pwm = (center_y_min - square_y) * 255 / center_y_min; // Movimento para cima
            } else {
                led_blue_pwm = (square_y - center_y_max) * 255 / (63 - center_y_max); // Movimento para baixo
            }
        }

        // Ajusta a intensidade dos LEDs via PWM
        if (leds_enabled) {
            pwm_set_gpio_level(LED_RED_PIN, led_red_pwm);
            pwm_set_gpio_level(LED_BLUE_PIN, led_blue_pwm);
        }

        sleep_ms(100);
    }
}


