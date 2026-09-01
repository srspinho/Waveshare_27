Versão de 02/08/2026
Inclui :
 - Debounce dos botões, por software
 - Menu dimensionado para 172 x 320
 - Persistência dos valores
 - Correção do Odometro Total e do Odomentro Parcial
 - To Do : Desenhar o enclosure

# Esquema Elétrico - HID Remapper & System Monitor (RP2350)

Projeto baseado na placa **Waveshare RP2350-LCD-1.47-A** com display ST7789 320×172 integrado, módulo RTC DS3231 e joystick de 5 vias.

Este documento descreve todas as conexões elétricas extraídas do código-fonte.

---

## Hardware Utilizado

| Componente              | Modelo / Descrição                          |
|-------------------------|---------------------------------------------|
| Placa principal         | Waveshare **RP2350-LCD-1.47-A**             |
| Display                 | ST7789 1.47" (320×172) - integrado         |
| RTC                     | DS3231 (módulo I2C)                        |
| Joystick                | 5 posições (UP / DOWN / LEFT / RIGHT / OK) |
| USB Host                | Pico-PIO-USB (nativo no RP2350)            |

---

## 1. Display (ST7789 - Integrado na placa)

| Função          | GPIO RP2350 | Observação                     |
|-----------------|-------------|-------------------------------|
| MOSI (DIN)      | **GP19**    | SPI0                          |
| SCLK            | **GP18**    | SPI0                          |
| CS              | **GP17**    |                               |
| DC              | **GP16**    |                               |
| RST             | **GP20**    |                               |
| **Backlight**   | **GP21**    | PWM (`TFT_BL`)                |

> O backlight é controlado via PWM no `GP21` através de um transistor SI2302 presente na placa.

---

## 2. Joystick (5 vias + OK)

Definido em `menu.h`:

| Botão       | GPIO    | Configuração no código     |
|-------------|---------|----------------------------|
| **UP**      | **GP3** | `INPUT_PULLUP`             |
| **DOWN**    | **GP2** | `INPUT_PULLUP`             |
| **LEFT**    | **GP6** | `INPUT_PULLUP`             |
| **RIGHT**   | **GP5** | `INPUT_PULLUP`             |
| **OK**      | **GP4** | `INPUT_PULLUP`             |
| **COMMON**  | **GP7** | Ligado ao **GND**          |

### Diagrama de ligação do Joystick

### Ligação do Joystick

| GPIO RP2350 | Sinal do Joystick    | Observação                 |
|-------------|----------------------|----------------------------|
| **GP3**     | UP                   | `INPUT_PULLUP`             |
| **GP2**     | DOWN                 | `INPUT_PULLUP`             |
| **GP6**     | LEFT                 | `INPUT_PULLUP`             |
| **GP5**     | RIGHT                | `INPUT_PULLUP`             |
| **GP4**     | OK (centro)          | `INPUT_PULLUP`             |
| **GP7**     | COMMON               | Ligado diretamente ao GND  |

Todos os botões utilizam pull-up interno. Ao serem pressionados, o pino é levado a GND.

---

## 3. RTC DS3231 (I2C1)

| Sinal RTC | GPIO RP2350 | Observação                  |
|-----------|-------------|-----------------------------|
| **SDA**   | **GP26**    | I2C1 (`Wire1`)              |
| **SCL**   | **GP27**    | I2C1 (`Wire1`)              |
| VCC       | 3.3V        |                             |
| GND       | GND         |                             |

> **Atenção**: Não utilize GP4/GP5 para o RTC.  
> Esses pinos são usados pelo joystick e o `setup_menu()` configura `pinMode()` neles.

---

## 4. USB Host (Pico-PIO-USB)

| Função            | GPIO     | Observação                              |
|-------------------|----------|-----------------------------------------|
| **USB Host D+**   | **GP0**  | `PIN_USB_HOST_DP`                       |
| **USB Host D-**   | **GP1**  | (D+ + 1)                                |
| **5V Enable**     | **GP18** | `PIN_5V_EN` (liga alimentação do Host)  |

O core1 é responsável pelo USB Host.  
O código possui proteção cooperativa (`g_flash_pausar_usb` / `g_flash_usb_parado`) para evitar travamento ao gravar na flash.

---

## 5. Outros Pinos Utilizados

| Função               | GPIO   | Uso no código                     |
|----------------------|--------|-----------------------------------|
| Seed aleatório       | GP28   | `analogRead(28)`                  |
| Backlight PWM        | GP21   | `analogWrite(TFT_BL, ...)`        |

---

## Visão Geral do Esquema

# Waveshare RP2350-LCD-1.47

| Dispositivo/Componente | Pinos RP2350         | Observações       |
|-------------------------|---------------------|-------------------|
| **USB-C**               | USB Device          | Conexão principal |
| **Teclado USB**          | GP0 (D+), GP1 (D-) | USB Host (PIO)    |
| **Controle de energia**  | GP18               | 5V_EN             |
| **Joystick**             | GP2                | DOWN              |
|                          | GP3                | UP                |
|                          | GP4                | OK                |
|                          | GP5                | RIGHT             |
|                          | GP6                | LEFT              |
|                          | GP7                | COMMON → GND      |
| **DS3231 RTC**           | GP26 SDA           | Wire1 (I2C1)      |
|                          | GP27 SCL           | Wire1 (I2C1)      |
|                          | 3V3 / GND          | Alimentação       |
| **LCD ST7789 (interno)** | GP16 DC            | Display Control   |
|                          | GP17 CS            | Chip Select       |
|                          | GP18 SCK           | Clock             |
|                          | GP19 MOSI          | Data              |
|                          | GP20 RST           | Reset             |
|                          | GP21 BL (PWM)      | Backlight         |

  <p Align="center">
  <img src="https://github.com/srspinho/Waveshare_26/blob/main/BCO.119462a6-2878-4ca1-a075-e63473203d3b.png" width="400">
  </p>

<p Align="center">
  <img src="https://github.com/srspinho/Waveshare_26/blob/main/BCO.32de3882-1623-4530-bb01-02336e455691.png" width="400">
  </p>
  

## Observações Importantes

1. **Conflito de pinos evitado**  
   O RTC foi movido corretamente para GP26/GP27, pois GP4 e GP5 são usados pelo joystick.

2. **USB Host + Flash**  
   O handshake entre core0 e core1 (`g_flash_pausar_usb`) é **obrigatório**. Sem ele a placa trava ao gravar na EEPROM.

3. **Controle de Brilho**  
   Realizado via PWM no GP21:
   ```cpp
   analogWrite(TFT_BL, map(config.sistema_brilho, 0, 100, 0, 255));


  <img src="https://github.com/srspinho/Waveshare_26/blob/main/WhatsApp Image 2026-08-18 at 18.28.11.jpeg" width="400">
  </p>
  <img src="https://github.com/srspinho/Waveshare_27/blob/main/WhatsApp Image 2026-09-01 at 18.37.25.jpeg" width="400"><img       src="https://github.com/srspinho/Waveshare_27/blob/main/WhatsApp Image 2026-09-01 at 19.19.26.jpeg" width="400">
  <img src="https://github.com/srspinho/Waveshare_27/blob/main/WhatsApp Image 2026-09-01 at 19.19.26 (1).jpeg" width="400">
  <img src="https://github.com/srspinho/Waveshare_27/blob/main/WhatsApp Image 2026-09-01 at 19.19.26 (2).jpeg" width="400">
  <img src="https://github.com/srspinho/Waveshare_27/blob/main/WhatsApp Image 2026-09-01 at 19.19.26 (3).jpeg" width="400">
  <img src="https://github.com/srspinho/Waveshare_27/blob/main/WhatsApp Image 2026-09-01 at 19.19.26 (4).jpeg" width="400">
  <img src="https://github.com/srspinho/Waveshare_27/blob/main/WhatsApp Image 2026-09-01 at 19.19.26 (5).jpeg" width="400">
  </p>
