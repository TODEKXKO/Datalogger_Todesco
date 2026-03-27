# 🧠 REGRAS DE HARDWARE E ARQUITETURA DO PROJETO (Datalogger Todesco)

## 1. Especificações da Placa
* Placa: Waveshare ESP32-S3 (esp32-s3-devkitc-1).
* Memória: 16MB Flash / 8MB PSRAM (OPI).
* Framework: Arduino Core 3.x (PioArduino v51.03.04).
* Display: ST7701 (RGB) + Touch CST820.

## 2. Bibliotecas Base
* Interface Gráfica: LVGL 8.4.0 (NÃO usar sintaxe da versão 9).
* WiFi/Internet: WiFiManager, ESP_Mail_Client, UniversalTelegramBot.

## 3. Gargalos e Soluções Conhecidas (NÃO ALTERAR)
* **Gargalo de PSRAM:** O rádio WiFi rouba a banda da PSRAM do display RGB, causando chuvisco e deslocamento de tela.
* **Solução da Tela (Bounce Buffer):** O `esp_lcd_rgb_panel_config_t` deve SEMPRE ter um `bounce_buffer_size_px` alto (ex: 40 * LCD_HEIGHT) alocado na SRAM para evitar perda de VSYNC. O `pclk_hz` deve ser reduzido se a imagem "rolar".
* **Solução do Código (Aguardar Internet):** O LVGL e as tarefas de WiFi rodam em núcleos separados via FreeRTOS. É obrigatório usar a macro `AGUARDAR_INTERNET()` para pausar as tarefas gráficas e de sensores durante a transmissão de rádio.