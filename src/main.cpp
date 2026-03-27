#include <Arduino.h>
#include "ui.h"
#include <Arduino.h>
#include "secrets.h"
#include "Wireless.h"
#include "Gyro_QMI8658.h"
#include "RTC_PCF85063.h"
#include "SD_Card.h"
#include "LVGL_Driver.h"
#include "BAT_Driver.h"
#include <Adafruit_SHT31.h>
#include <SD.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <ESP_Mail_Client.h>
#include <esp_task_wdt.h>
#include "time.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ==========================================
// LOG NO SERIAL MONITOR
// ==========================================
#define LOG_FMT(nivel, fmt, ...) Serial.printf("[%02d:%02d:%02d] [" nivel "] " fmt "\n", datetime.hour, datetime.minute, datetime.second, ##__VA_ARGS__)
#define LOG_INFO(msg)  LOG_FMT("INFO ", "%s", msg)
#define LOG_WARN(msg)  LOG_FMT("AVISO", "%s", msg)
#define LOG_ERR(msg)   LOG_FMT("ERRO ", "%s", msg)

// ==========================================
// EMAIL
// ==========================================
#define SMTP_HOST       "smtp.gmail.com"
#define SMTP_PORT       465
SMTPSession smtp;

// ==========================================
// TELEGRAM
// ==========================================
#define TELEGRAM_HOST   "api.telegram.org"
#define TELEGRAM_PORT   443

WiFiClientSecure clientSecure;
UniversalTelegramBot bot(BOT_TOKEN, clientSecure);

WiFiClientSecure clientPersistente;
bool     conexao_persistente_ok = false;
uint32_t tempo_ultima_atividade = 0;
#define  TIMEOUT_PERSISTENTE 60000

String chat_id_salvo = "";

// ==========================================
// FILA DE MENSAGENS PARA O CORE 0
// ==========================================
struct MensagemBot {
  char chat_id[32];
  char texto[512];
  char parse_mode[16];
};
QueueHandle_t fila_bot = NULL;

void Enfileirar_Alerta(const String& texto, const String& parse_mode = "Markdown") {
  if (chat_id_salvo.length() == 0 || fila_bot == NULL) return;
  MensagemBot msg;
  chat_id_salvo.toCharArray(msg.chat_id, sizeof(msg.chat_id));
  texto.toCharArray(msg.texto, sizeof(msg.texto));
  parse_mode.toCharArray(msg.parse_mode, sizeof(msg.parse_mode));
  if (xQueueSendToBack(fila_bot, &msg, 0) != pdTRUE) {
    Serial.println("[FILA] AVISO: fila cheia, alerta descartado");
  }
}

// ==========================================
// SINCRONIZACAO CORE 0 <-> CORE 1
// ==========================================
SemaphoreHandle_t lvgl_mutex = NULL;

// ==========================================
// LIMITES E FLAGS DE ALARME
// ==========================================
float limite_temp_max = 30.0;
float limite_temp_min = 20.0;
float limite_umid_max = 90.0;
bool  alerta_temp_disparado = false;
bool  alerta_umid_disparado = false;

// ==========================================
// BATERIA E ENERGIA
// ==========================================
float voltagem_bateria     = 0.0;
int   porcentagem_bateria  = 0;
bool  usb_conectado        = true;
bool  energia_caiu_alerta  = false;
bool  primeira_leitura_bat = true;
int   contador_bat_baixa   = 0;

volatile bool evento_energia_caiu   = false;
volatile bool evento_energia_voltou = false;

// ==========================================
// BUZZER
// ==========================================
uint32_t tempo_inicio_alta_temp = 0;
bool     buzzer_ja_apitou       = false;

// ==========================================
// VARIAVEIS GLOBAIS
// ==========================================
#define PIN_LUZ_TELA 6

Adafruit_SHT31 sht31 = Adafruit_SHT31();
Preferences    preferencias;

float t_alvo     = -999.0;
float h_alvo     = -999.0;
float arco_suave = 0;
bool  wifi_ok           = false;
bool  sd_ok             = false;
bool  hora_atualizada   = false;
bool  sensor_ok         = false;
bool  pedir_portal_wifi = false;
bool  portal_aberto     = false;
bool  reiniciando       = false;

char email_salvo[50]  = "";
int  brilho_atual_pwm = 255;

lv_chart_series_t *ser_temp;
WiFiManager wm;
WiFiManagerParameter custom_email("email", "E-mail para relatorios", email_salvo, 50);
const long gmtOffset_sec = -10800;

// ==========================================
// QR CODE TELEGRAM E WIFI
// ==========================================
void Gerar_QRCode_Telegram() {
  if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    lv_obj_clean(ui_uiQRArea);
    String mac_limpo = WiFi.macAddress();
    mac_limpo.replace(":", "");
    String link_bot = "https://t.me/DataloggerTodescoBot?start=" + mac_limpo;
    lv_obj_t *qr = lv_qrcode_create(ui_uiQRArea, 260,
                                     lv_color_hex(0x000000),
                                     lv_color_hex(0xFFFFFF));
    lv_qrcode_update(qr, link_bot.c_str(), link_bot.length());
    lv_obj_center(qr);
    xSemaphoreGive(lvgl_mutex);
  }
}

static void wifi_qr_close_cb(lv_event_t * e) {
    lv_obj_add_flag(ui_uiPanelSetupWiFi, LV_OBJ_FLAG_HIDDEN);
}

void Gerar_QRCode_WiFi() {
  if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    lv_obj_clean(ui_uiPanelQRWiFi);
    
    // Ajuste Automático: Força o painel a ficar perfeitamente no centro
    lv_obj_set_align(ui_uiPanelQRWiFi, LV_ALIGN_CENTER);
    lv_obj_set_y(ui_uiPanelQRWiFi, 0);

    String wifi_str = "https://todekxko.github.io/Datalogger_Todesco/?mac=" + WiFi.macAddress();
    lv_obj_t *qr = lv_qrcode_create(ui_uiPanelQRWiFi, 275,
                                     lv_color_black(),
                                     lv_color_white());
    lv_qrcode_update(qr, wifi_str.c_str(), wifi_str.length());
    lv_obj_center(qr);
    
    lv_obj_add_flag(ui_uiPanelQRWiFi, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_uiPanelQRWiFi, wifi_qr_close_cb, LV_EVENT_CLICKED, NULL);
    
    xSemaphoreGive(lvgl_mutex);
  }
}

// ==========================================
// CONEXAO PERSISTENTE SSL
// ==========================================
bool Garantir_Conexao_Persistente() {
  if (conexao_persistente_ok &&
      clientPersistente.connected() &&
      (millis() - tempo_ultima_atividade < TIMEOUT_PERSISTENTE)) {
    return true;
  }
  Serial.println("[SSL] Abrindo conexao persistente...");
  clientPersistente.stop();
  clientPersistente.setInsecure();
  clientPersistente.setTimeout(8);
  clientPersistente.setHandshakeTimeout(8);
  uint32_t t0 = millis();
  if (!clientPersistente.connect(TELEGRAM_HOST, TELEGRAM_PORT)) {
    Serial.printf("[SSL] ERRO: Falha na conexao (%lums)\n", millis() - t0);
    conexao_persistente_ok = false;
    return false;
  }
  Serial.printf("[SSL] Conexao estabelecida em %lums\n", millis() - t0);
  conexao_persistente_ok = true;
  tempo_ultima_atividade = millis();
  return true;
}

// ==========================================
// ENVIO DE MENSAGEM COM TELA PROTEGIDA
// ==========================================
void Enviar_Mensagem_Bot(const String& chat, const String& texto, const String& modo) {
  bot.sendMessage(chat, texto, modo);
  tempo_ultima_atividade = millis();
}

// ==========================================
// TAREFA DE INTERNET - CORE 0
// ==========================================
void Tarefa_Internet(void *parameter) {
  static uint32_t timer_telegram = 0;
  static uint32_t timer_email    = 0;

  Serial.println("[TASK] Tarefa_Internet iniciada no Core 0");
  vTaskDelay(pdMS_TO_TICKS(5000));
  Serial.println("[TASK] Inicio do polling");

  while (1) {

    // Suspende tudo enquanto portal WiFi estiver aberto
    if (portal_aberto) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    // Drena fila de alertas com pausa entre mensagens
    if (wifi_ok && fila_bot != NULL) {
      MensagemBot msg;
      while (xQueueReceive(fila_bot, &msg, 0) == pdTRUE) {
        Serial.printf("[FILA] Enviando: %.60s\n", msg.texto);
        uint32_t t0 = millis();
        Enviar_Mensagem_Bot(msg.chat_id, msg.texto, msg.parse_mode);
        Serial.printf("[FILA] Enviado em %lums\n", millis() - t0);
        // Pausa entre mensagens
        vTaskDelay(pdMS_TO_TICKS(500));
      }
    }

    // Alertas de energia
    if (wifi_ok && chat_id_salvo.length() > 0) {
      if (evento_energia_caiu) {
        evento_energia_caiu = false;
        Serial.println("[ENERGIA] Enviando alerta de queda...");
        String min_str = (datetime.minute < 10)
                         ? "0" + String(datetime.minute) : String(datetime.minute);
        String msg = "*ALERTA DE QUEDA DE ENERGIA!*\n\nRodando na bateria.\n\nBateria: "
                     + String(porcentagem_bateria) + "%\nHora: "
                     + String(datetime.hour) + ":" + min_str;
        Enviar_Mensagem_Bot(chat_id_salvo, msg, "Markdown");
      }
      if (evento_energia_voltou) {
        evento_energia_voltou = false;
        Serial.println("[ENERGIA] Enviando retorno...");
        Enviar_Mensagem_Bot(chat_id_salvo,
          "*ENERGIA RESTABELECIDA!*\n\nVoltou a receber energia da tomada.",
          "Markdown");
      }
    }

    // Polling Telegram a cada 30s com conexao persistente
    if (millis() - timer_telegram > 30000) {
      if (wifi_ok) {
        Serial.println("[TELEGRAM] Iniciando polling...");

        uint32_t t_ssl = millis();
          bool conn_ok = Garantir_Conexao_Persistente();

          if (conn_ok) {
            int n = bot.getUpdates(bot.last_message_received + 1);
            uint32_t dur = millis() - t_ssl;
            Serial.printf("[TELEGRAM] getUpdates em %lums - %d msg(s)\n", dur, n);

            while (n) {
              for (int i = 0; i < n; i++) {
                String cid       = bot.messages[i].chat_id;
                String text      = bot.messages[i].text;
                String from_name = bot.messages[i].from_name;
                Serial.printf("[TELEGRAM] Msg de %s: %s\n",
                              from_name.c_str(), text.c_str());

                if (text.startsWith("/start")) {
                  chat_id_salvo = cid;
                  preferencias.begin("datalogger", false);
                  preferencias.putString("chat_id", chat_id_salvo);
                  preferencias.end();
                  bot.sendMessage(cid,
                    "Ola, " + from_name + "!\n\nCelular pareado com sucesso.", "");
                  Serial.println("[TELEGRAM] /start processado");

                  // Esconder a tela do QRCode e ir pra tela principal
                  if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    lv_obj_add_flag(ui_uiPanelTelegram, LV_OBJ_FLAG_HIDDEN);
                    _ui_screen_change(&ui_uiScreenMain, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0, &ui_uiScreenMain_screen_init);
                    xSemaphoreGive(lvgl_mutex);
                  }
                }
                else if (text == "/status") {
                  String s = "*STATUS DO EQUIPAMENTO*\n\n";
                  s += "Temperatura: " + String(t_alvo, 1) + "C\n";
                  s += "Umidade: "     + String((int)h_alvo) + "%\n\n";
                  if (usb_conectado) {
                    s += "Energia: Na Tomada\nBateria: Carregando...\n";
                  } else {
                    s += "Energia: Bateria\nBateria: "
                         + String(porcentagem_bateria) + "% ("
                         + String(voltagem_bateria, 2) + "V)\n";
                  }
                  bot.sendMessage(cid, s, "Markdown");
                  Serial.println("[TELEGRAM] /status enviado");
                }
              }
              n = bot.getUpdates(bot.last_message_received + 1);
            }
            tempo_ultima_atividade = millis();
          } else {
            conexao_persistente_ok = false;
            Serial.println("[TELEGRAM] Sem conexao - tentando na proxima rodada");
          }
        
        Serial.println("[TELEGRAM] Polling concluido");
      } else {
        Serial.println("[TELEGRAM] WiFi offline - polling ignorado");
      }
      timer_telegram = millis();
    }

    // Email a cada 1h
    if (millis() - timer_email > 3600000) {
      if (wifi_ok && strlen(email_salvo) > 0) {
        Serial.println("[EMAIL] Iniciando envio...");

        ESP_Mail_Session session;
          session.server.host_name  = SMTP_HOST;
          session.server.port       = SMTP_PORT;
          session.login.email       = REMETENTE_EMAIL;
          session.login.password    = REMETENTE_SENHA;
          session.login.user_domain = "";

          SMTP_Message message;
          message.sender.name  = "Todesco Datalogger";
          message.sender.email = REMETENTE_EMAIL;
          message.subject      = "Relatorio de Temperatura";
          message.addRecipient("Cliente", email_salvo);
          message.html.content = "<h2>Ola!</h2><p>Em anexo esta o seu relatorio.</p>";

          String csvData = "";
          File file = SD_MMC.open("/dados.csv", FILE_READ);
          if (file) {
            csvData = file.readString();
            file.close();
            Serial.printf("[EMAIL] CSV lido: %d bytes\n", csvData.length());
            SMTP_Attachment att;
            att.descr.filename = "relatorio.csv";
            att.descr.mime     = "text/csv";
            att.blob.data      = (uint8_t *)csvData.c_str();
            att.blob.size      = csvData.length();
            message.addAttachment(att);
          } else {
            Serial.println("[EMAIL] AVISO: CSV nao encontrado");
          }

          if (smtp.connect(&session)) {
            if (MailClient.sendMail(&smtp, &message)) {
              Serial.println("[EMAIL] Enviado com sucesso");
              if (sd_ok) {
                SD_MMC.mkdir("/enviados");
                char novo_nome[64];
                sprintf(novo_nome, "/enviados/log_%02d%02d%04d_%02d%02d.csv",
                        datetime.day, datetime.month, datetime.year + 2000,
                        datetime.hour, datetime.minute);
                SD_MMC.rename("/dados.csv", novo_nome);
                File f = SD_MMC.open("/dados.csv", FILE_WRITE);
                if (f) { f.println("Data,Hora,Temperatura,Umidade"); f.close(); }
              }
            } else {
              Serial.println("[EMAIL] ERRO: Falha no envio");
            }
          } else {
            Serial.println("[EMAIL] ERRO: Falha SMTP");
          }
        
      }
      timer_email = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ==========================================
// ALERTAS
// ==========================================
void Processar_Alertas() {
  if (!sensor_ok) return;
  if (chat_id_salvo.length() == 0) return;

  String min_str = (datetime.minute < 10)
                   ? "0" + String(datetime.minute) : String(datetime.minute);

  if (t_alvo >= limite_temp_max || t_alvo <= limite_temp_min) {
    if (tempo_inicio_alta_temp == 0) {
      tempo_inicio_alta_temp = millis();
      Serial.printf("[ALERTA] Temp fora do limite: %.1fC - aguardando 15s\n", t_alvo);
    }
    if ((millis() - tempo_inicio_alta_temp >= 15000) && !alerta_temp_disparado) {
      Serial.printf("[ALERTA] Disparando alerta temp: %.1fC\n", t_alvo);
      String msg = "*ALERTA CRITICO DE TEMPERATURA!*\n\nFora da faixa segura!\n\nAtual: "
                   + String(t_alvo, 1) + "C\nHora: "
                   + String(datetime.hour) + ":" + min_str;
      Enfileirar_Alerta(msg);
      alerta_temp_disparado = true;
      if (!buzzer_ja_apitou) {
        for (int ciclo = 0; ciclo < 2; ciclo++) {
          for (int i = 0; i < 3; i++) {
            Set_EXIO(EXIO_PIN8, High); delay(150);
            Set_EXIO(EXIO_PIN8, Low);  delay(150);
          }
          if (ciclo == 0) delay(800);
        }
        buzzer_ja_apitou = true;
      }
    }
  }
  else if (t_alvo < limite_temp_max && t_alvo > limite_temp_min) {
    tempo_inicio_alta_temp = 0;
    buzzer_ja_apitou       = false;
    if (alerta_temp_disparado) {
      Serial.printf("[ALERTA] Temp normalizada: %.1fC\n", t_alvo);
      Enfileirar_Alerta("*TEMPERATURA NORMALIZADA*\n\nAtual: "
                        + String(t_alvo, 1) + "C");
      alerta_temp_disparado = false;
    }
  }

  if (h_alvo >= limite_umid_max && !alerta_umid_disparado) {
    Serial.printf("[ALERTA] Umidade alta: %d%%\n", (int)h_alvo);
    String msg = "*ALERTA DE UMIDADE!*\n\nAtual: " + String((int)h_alvo)
                 + "%\nHora: " + String(datetime.hour) + ":" + min_str;
    Enfileirar_Alerta(msg);
    alerta_umid_disparado = true;
  }
  else if (h_alvo <= (limite_umid_max - 5.0) && alerta_umid_disparado) {
    Serial.printf("[ALERTA] Umidade normalizada: %d%%\n", (int)h_alvo);
    Enfileirar_Alerta("*UMIDADE NORMALIZADA*\n\nAtual: "
                      + String((int)h_alvo) + "%");
    alerta_umid_disparado = false;
  }
}

// ==========================================
// CALLBACKS DA UI
// ==========================================
void onWiFiEvent(WiFiEvent_t event) { }

void saveConfigCallback() {
  strcpy(email_salvo, custom_email.getValue());
  preferencias.begin("datalogger", false);
  preferencias.putString("email", email_salvo);
  preferencias.end();
  Serial.printf("[WIFI] Email salvo: %s\n", email_salvo);
  reiniciando = true;
}

void reset_logs_action(lv_event_t *e) {
  if (sd_ok) {
    SD_MMC.remove("/dados.csv");
    File file = SD_MMC.open("/dados.csv", FILE_WRITE);
    if (file) { file.println("Data,Hora,Temperatura,Umidade"); file.close(); }
    
    // Como os eventos do LVGL são chamados DE DENTRO do Lvgl_Loop(), o lvgl_mutex já está tomado!
    // Não precisamos (nem podemos) tentar pegar o mutex de novo aqui, ou vai dar timeout e a tela não atualiza.
    lv_chart_set_all_value(ui_uichartTemp, ser_temp, LV_CHART_POINT_NONE);
    lv_chart_refresh(ui_uichartTemp);
    lv_obj_add_flag(ui_uiPanelConfirmReset, LV_OBJ_FLAG_HIDDEN);
    
    Serial.println("[SD] Logs resetados");
  }
}

void mudar_brilho_action(lv_event_t *e) {
  int v = lv_slider_get_value(lv_event_get_target(e));
  brilho_atual_pwm = map(v, 0, 100, 10, 255);
  analogWrite(PIN_LUZ_TELA, brilho_atual_pwm);
}

void Atualizar_Popup_Infos(lv_event_t *e) {
  String ip   = WiFi.localIP().toString();
  String rede = WiFi.SSID();
  String sd_txt = "Erro/Sem SD";
  if (sd_ok) {
    uint64_t total = SD_MMC.totalBytes() / (1024 * 1024);
    uint64_t used  = SD_MMC.usedBytes()  / (1024 * 1024);
    sd_txt = String(used) + "MB / " + String(total) + "MB";
  }
  
  // Sem mutex: chamado dentro do Lvgl_Loop que já está protegido
  lv_label_set_text(ui_uiLabelIP,       ip.c_str());
  lv_label_set_text(ui_uiLabelSSID,     rede.c_str());
  lv_label_set_text(ui_uiLabelSDStatus, sd_txt.c_str());
  lv_label_set_text(ui_uiLabelVersion,  "v1.0.0");
  lv_obj_clear_flag(ui_uiPanelInfos, LV_OBJ_FLAG_HIDDEN);
}

void ativar_setup_wifi(lv_event_t *e) {
  // Sem mutex: chamado dentro do Lvgl_Loop que já está protegido
  lv_obj_clear_flag(ui_uiPanelSetupWiFi, LV_OBJ_FLAG_HIDDEN);
  pedir_portal_wifi = true;
}

// ==========================================
// HARDWARE
// ==========================================
void Driver_Init() {
  Flash_test();
  BAT_Init();
  I2C_Init();
  TCA9554PWR_Init(0x00);
  Set_EXIO(EXIO_PIN8, Low);
  PCF85063_Init();
  QMI8658_Init();
}

void Sincronizar_Relogio() {
  Serial.println("[NTP] Sincronizando relogio...");
  configTime(gmtOffset_sec, 0, "a.st1.ntp.br", "pool.ntp.org");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 8000)) {
    datetime.year   = timeinfo.tm_year + 1900 - 2000;
    datetime.month  = timeinfo.tm_mon + 1;
    datetime.day    = timeinfo.tm_mday;
    datetime.hour   = timeinfo.tm_hour;
    datetime.minute = timeinfo.tm_min;
    datetime.second = timeinfo.tm_sec;
    PCF85063_Set_Time(datetime);
    hora_atualizada = true;
    Serial.printf("[NTP] Sincronizado: %02d:%02d:%02d %02d/%02d/%04d\n",
                  datetime.hour, datetime.minute, datetime.second,
                  datetime.day, datetime.month, datetime.year + 2000);
  } else {
    Serial.println("[NTP] ERRO: Falha na sincronizacao");
  }
}

// ==========================================
// GRAFICO E SD
// ==========================================
void Inicializar_Grafico_UI() {
  if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ser_temp = lv_chart_add_series(ui_uichartTemp,
                                    lv_palette_main(LV_PALETTE_RED),
                                    LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_line_width(ui_uichartTemp, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(ui_uichartTemp, 0, LV_PART_INDICATOR);
    lv_chart_set_point_count(ui_uichartTemp, 144);
    lv_chart_set_range(ui_uichartTemp, LV_CHART_AXIS_PRIMARY_Y, 20, 35);
    xSemaphoreGive(lvgl_mutex);
  }
}

void Carregar_Historico_SD() {
  Serial.println("[SD] Carregando historico...");
  File file = SD_MMC.open("/dados.csv", FILE_READ);
  if (!file) {
    Serial.println("[SD] Historico nao encontrado");
    return;
  }
  if (file.available()) file.readStringUntil('\n');
  int linhas = 0;
  while (file.available()) {
    String linha = file.readStringUntil('\n');
    int v1 = linha.indexOf(',');
    int v2 = linha.indexOf(',', v1 + 1);
    int v3 = linha.indexOf(',', v2 + 1);
    if (v3 > 0) {
      float t = linha.substring(v2 + 1, v3).toFloat();
      if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        lv_chart_set_next_value(ui_uichartTemp, ser_temp, (int)t);
        xSemaphoreGive(lvgl_mutex);
      }
      linhas++;
    }
    yield();
  }
  file.close();
  if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    lv_chart_refresh(ui_uichartTemp);
    xSemaphoreGive(lvgl_mutex);
  }
  Serial.printf("[SD] Historico carregado: %d registros\n", linhas);
}

void Salvar_Historico_SD() {
  File file = SD_MMC.open("/dados.csv", FILE_APPEND);
  if (file) {
    if (file.size() == 0) file.println("Data,Hora,Temperatura,Umidade");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char linha[64];
      sprintf(linha, "%02d/%02d/%04d,%02d:%02d:%02d,%.2f,%.2f",
              timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
              timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
              t_alvo, h_alvo);
      file.println(linha);
      file.flush();
      if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        lv_chart_set_next_value(ui_uichartTemp, ser_temp, (int)t_alvo);
        lv_chart_refresh(ui_uichartTemp);
        xSemaphoreGive(lvgl_mutex);
      }
      Serial.printf("[SD] Salvo: %s\n", linha);
    }
    file.close();
  } else {
    Serial.println("[SD] ERRO: Falha ao abrir CSV");
  }
}

// ==========================================
// SISTEMA PRINCIPAL - CORE 1
// ==========================================
void Atualizar_Sistema() {
  static uint32_t timer_1s    = 0;
  static uint32_t timer_100ms = 0;
  static uint32_t timer_wifi  = 0;
  static uint32_t timer_sd    = 0;
  static uint32_t timer_bat   = 0;
  static bool blink = false;

  if (millis() - timer_100ms > 100) {
    timer_100ms = millis();
    QMI8658_Loop();
    RTC_Loop();
  }

  if (millis() - timer_bat > 5000) {
    timer_bat = millis();
    voltagem_bateria = BAT_Get_Volts();

    // Reajuste das faixas para detectar carregador vs bateria
    if      (voltagem_bateria >= 4.18) porcentagem_bateria = 100;
    else if (voltagem_bateria <= 3.20) porcentagem_bateria = 0;
    else    porcentagem_bateria = map((int)(voltagem_bateria * 100), 320, 418, 0, 100);

    bool estado_anterior = usb_conectado;

    // Quando conectado na USB, o C.I TP4056 sobe a tensão para ~4.22V
    if (voltagem_bateria >= 4.20) {
      contador_bat_baixa = 0;
      usb_conectado = true;
    } else if (voltagem_bateria < 4.10) {
      // Ao retirar o cabo USB e a tela puxando carga, ele desce p/ < 4.10V
      // Aguardamos 5 leituras (~25 seg) para confirmar e evitar falsos alarmes
      if (++contador_bat_baixa >= 5) usb_conectado = false;
    }

    Serial.printf("[BAT] %.2fV - %d%% - USB: %s\n",
                  voltagem_bateria, porcentagem_bateria,
                  usb_conectado ? "sim" : "nao");

    if (primeira_leitura_bat) {
      primeira_leitura_bat = false;
      energia_caiu_alerta  = !usb_conectado;
    } else {
      if (!usb_conectado && estado_anterior && !energia_caiu_alerta) {
        energia_caiu_alerta = true;
        evento_energia_caiu = true;
        Serial.println("[BAT] ATENCAO: Energia caiu");
      } else if (usb_conectado && !estado_anterior && energia_caiu_alerta) {
        energia_caiu_alerta   = false;
        evento_energia_voltou = true;
        Serial.println("[BAT] Energia restabelecida");
      }
    }
  }

  Processar_Alertas();

  if (millis() - timer_sd > 35000) {
    timer_sd = millis();
    if (sd_ok && hora_atualizada && sensor_ok) Salvar_Historico_SD();
  }

  if (millis() - timer_wifi > 2000) {
    timer_wifi = millis();
    bool estava_ok = wifi_ok;
    wifi_ok = (WiFi.status() == WL_CONNECTED);
    if (estava_ok && !wifi_ok) {
      Serial.println("[WIFI] ATENCAO: Desconectado");
      conexao_persistente_ok = false;
    }
    if (!estava_ok && wifi_ok) Serial.println("[WIFI] Reconectado");
    if (wifi_ok && !hora_atualizada) Sincronizar_Relogio();
    if (wifi_ok && portal_aberto) {
      wm.stopConfigPortal();
      portal_aberto = false;
      if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        lv_obj_add_flag(ui_uiPanelSetupWiFi, LV_OBJ_FLAG_HIDDEN);
        xSemaphoreGive(lvgl_mutex);
      }
      Serial.println("[WIFI] Portal fechado apos conexao");
    }
  }

  if (millis() - timer_1s > 1000) {
    timer_1s = millis();
    blink = !blink;

    float t = sht31.readTemperature();
    float h = sht31.readHumidity();

    if (!isnan(t) && t > -40.0 && !isnan(h)) {
      t_alvo    = t;
      h_alvo    = h;
      sensor_ok = true;
    } else {
      Serial.println("[SHT31] ERRO: Leitura invalida");
    }

    if (sensor_ok) {
      if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        lv_label_set_text(ui_uilabelTemp, (String(t_alvo, 1) + "C").c_str());
        lv_label_set_text_fmt(ui_uilabelUmid, "%d%%", (int)h_alvo);
        xSemaphoreGive(lvgl_mutex);
      }
    } else {
      if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        lv_label_set_text(ui_uilabelTemp, "--.-C");
        lv_label_set_text(ui_uilabelUmid, "--%");
        xSemaphoreGive(lvgl_mutex);
      }
    }

    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      lv_label_set_text_fmt(ui_uilabelTime, "%02d:%02d",
                            datetime.hour, datetime.minute);
      lv_obj_set_style_text_opa(ui_uiiconWiFi,
                                wifi_ok ? 255 : (blink ? 255 : 50), 0);
      lv_obj_set_style_text_opa(ui_uiiconSD,
                                sd_ok   ? 255 : (blink ? 255 : 50), 0);
      xSemaphoreGive(lvgl_mutex);
    }
  }
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  lvgl_mutex = xSemaphoreCreateMutex();

  Serial.begin(115200);
  delay(500);

  Serial.println("\n=============================");
  Serial.println("   TODESCO DATALOGGER v1.0   ");
  Serial.println("=============================");

  esp_reset_reason_t motivo = esp_reset_reason();
  Serial.print("[BOOT] Motivo do reset: ");
  switch (motivo) {
    case ESP_RST_POWERON:   Serial.println("Energia ligada (normal)");        break;
    case ESP_RST_SW:        Serial.println("Reset por software");              break;
    case ESP_RST_PANIC:     Serial.println("ERRO - PANIC/Exception");          break;
    case ESP_RST_INT_WDT:   Serial.println("ERRO - Watchdog de interrupcao");  break;
    case ESP_RST_TASK_WDT:  Serial.println("ERRO - Watchdog de task");         break;
    case ESP_RST_WDT:       Serial.println("ERRO - Watchdog outro");           break;
    case ESP_RST_BROWNOUT:  Serial.println("ERRO - BROWNOUT queda de tensao"); break;
    default:                Serial.printf("Desconhecido (%d)\n", motivo);      break;
  }

  Serial.printf("[BOOT] Heap livre:  %d bytes\n", ESP.getFreeHeap());
  Serial.printf("[BOOT] PSRAM livre: %d bytes\n", ESP.getFreePsram());
  Serial.printf("[BOOT] Chip: %s Rev%d - %dMHz\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz());
  Serial.println("=============================\n");

  // Iniciar BLE Trovão imediatamente antes de sobrecarregar com a Tela
  Iniciar_BLE_Provisionamento();
  delay(500);

  pinMode(PIN_LUZ_TELA, OUTPUT);
  analogWrite(PIN_LUZ_TELA, 255);

  Serial.println("[SETUP] Inicializando drivers...");
  Driver_Init();

  WiFi.onEvent(onWiFiEvent);
  clientSecure.setInsecure();
  clientSecure.setTimeout(5);
  clientSecure.setHandshakeTimeout(5);

  preferencias.begin("datalogger", true);
  String email_temp    = preferencias.getString("email",   "");
  String telegram_temp = preferencias.getString("chat_id", "");
  preferencias.end();
  if (email_temp.length() > 0) {
    strcpy(email_salvo, email_temp.c_str());
    Serial.printf("[PREFS] Email carregado: %s\n", email_salvo);
  }
  if (telegram_temp.length() > 0) {
    chat_id_salvo = telegram_temp;
    Serial.printf("[PREFS] Chat ID carregado: %s\n", chat_id_salvo.c_str());
  }

  Wire.setClock(100000);
  if (!sht31.begin(0x44)) {
    Serial.println("[SHT31] ERRO: Sensor nao encontrado em 0x44");
  } else {
    Serial.println("[SHT31] Sensor iniciado");
  }

  Serial.println("[SETUP] Iniciando LCD e LVGL...");
  LCD_Init();
  Lvgl_Init();
  ui_init();
  Serial.println("[SETUP] LCD e LVGL prontos");

  Serial.println("[SETUP] Iniciando SD...");
  SD_Init();
  if (SD_MMC.cardType() != CARD_NONE) {
    sd_ok = true;
    Serial.println("[SD] Cartao detectado");
    Inicializar_Grafico_UI();
    Carregar_Historico_SD();
  } else {
    Serial.println("[SD] AVISO: Cartao nao detectado");
  }

  wm.addParameter(&custom_email);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(180);

  WiFi.mode(WIFI_STA);
  WiFi.begin();
  
  // Forçando o No-Sleep do Wi-Fi para evitar o bug de scroll / jitter do ST7701
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE); 
  
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  WiFi.setTxPower(WIFI_POWER_5dBm);
  Serial.println("[WIFI] Tentando conectar...");

  // Aguardar até 5 segundos para ver se o WiFi conecta usando as credenciais salvas
  uint32_t t_wifi = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t_wifi < 5000)) {
      delay(100);
      Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WIFI] Conectado com sucesso!");
  } else {
      Serial.println("[WIFI] Falha ao conectar. Trabalhando apenas como servidor BLE agora.");
  }

  if (!MDNS.begin("datalogger")) Serial.println("[mDNS] AVISO: Falha");
  else Serial.println("[mDNS] Iniciado: datalogger.local");

  fila_bot = xQueueCreate(10, sizeof(MensagemBot));
  Serial.println("[FILA] Fila de mensagens criada");

  xTaskCreatePinnedToCore(Tarefa_Internet, "Internet Task", 16384, NULL, 1, NULL, 0);
  Serial.println("[TASK] Tarefa de internet criada no Core 0");

  Gerar_QRCode_Telegram();
  Gerar_QRCode_WiFi();
  if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    lv_obj_clear_flag(ui_uiarcTemp, LV_OBJ_FLAG_CLICKABLE);
    xSemaphoreGive(lvgl_mutex);
  }

  Serial.println("[SETUP] Concluido - sistema rodando\n");
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  if (!is_BLE_Provisionamento_Conectado()) {
    wm.process();
  }

  if (pedir_portal_wifi && !portal_aberto) {
    pedir_portal_wifi      = false;
    portal_aberto          = true;
    conexao_persistente_ok = false;
    Serial.println("[WIFI] Abrindo portal nao-bloqueante...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();
    wm.startConfigPortal("Todesco_Datalogger");
  }

  if (reiniciando) {
    Serial.println("[BOOT] Reiniciando...");
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP.restart();
  }

  // Garante prioridade total à renderização (aguarda o mutex o tempo que for necessário)
  // para evitar que atrasos de WiFi/Telegram causem perda de sincronismo e gerem o bug de scroll
  if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
    Lvgl_Loop();
    xSemaphoreGive(lvgl_mutex);
  }

  Atualizar_Sistema();

  if (sensor_ok) {
    arco_suave += (t_alvo - arco_suave) * 0.05f;
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      lv_arc_set_value(ui_uiarcTemp, (int)(arco_suave * 10.0f));
      xSemaphoreGive(lvgl_mutex);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(5));
}
