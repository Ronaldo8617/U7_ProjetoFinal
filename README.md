# Projeto Raspberry Pi Pico: Sistema Embarcado para Monitoramento de Temperatura, Umidade e Ruído

Este projeto foi desenvolvido utilizando o **Raspberry Pi Pico** e tem como objetivo monitorar **temperatura**, **umidade** e **nível de ruído** em tempo real. O sistema utiliza um joystick para simular a leitura de temperatura e umidade, um módulo MIC para capturar o nível de ruído, e um display OLED SSD1306 para exibir os dados. Além disso, o sistema emite alertas visuais e sonoros quando os valores ultrapassam limites críticos configuráveis.

---

## Funcionalidades

- **Leitura de Sensores**:
  - Simulação de temperatura e umidade via joystick (entradas analógicas).
  - Captura de ruído via módulo MIC (entrada analógica).

- **Alertas**:
  - **Buzzer**: Acionado quando os valores de temperatura, umidade ou ruído ultrapassam os limites críticos.
  - **LEDs**: Indicadores visuais (vermelho e verde) para alertas de condições críticas.

- **Interface Gráfica**:
  - Exibição dos valores atuais de temperatura, umidade e ruído em um display OLED.

- **Armazenamento de Dados**:
  - Armazenamento das médias de temperatura, umidade e ruído a cada 3 segundos.
  - Capacidade de armazenar até 365 "dias" de dados.

- **Controle por Botões**:
  - **Botão A**: Permite a atualização dos limites críticos via comunicação serial.
  - **Botão B**: Imprime os dados acumulados no Serial Monitor.
  - **Botão do Joystick**: Ativa/desativa o override do ruído.

- **Comunicação Serial**:
  - Envio dos dados para o Serial Monitor.
  - Recebimento de comandos para atualização dos limites críticos.

---

## Como Usar

1. **Interação com o Sistema**:
   - Pressione o **Botão A** para atualizar os limites críticos de temperatura, umidade e ruído via comunicação serial.
   - Pressione o **Botão B** para imprimir os dados acumulados no Serial Monitor.
   - Pressione o **Botão do Joystick** para ativar/desativar o override do ruído (força o nível de ruído para 100%).

2. **Monitoramento**:
   - Conecte-se ao Serial Monitor para visualizar os valores atuais de temperatura, umidade e ruído.
   - O display OLED exibirá os valores em tempo real.

3. **Alertas**:
   - Quando os valores ultrapassam os limites críticos, o buzzer é acionado e os LEDs piscam em padrões específicos.

---

## Bibliotecas Usadas

- `ssd1306`: Biblioteca para controle do display OLED SSD1306.
- `pico/stdlib`: Biblioteca padrão do Raspberry Pi Pico.
- `hardware/adc`: Biblioteca para leitura de entradas analógicas.
- `hardware/i2c`: Biblioteca para comunicação I2C.
- `hardware/pwm`: Biblioteca para controle de PWM (LEDs).

---

## Vídeo Demonstrativo

https://youtu.be/H3M8OSEebTQ



